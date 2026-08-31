"""
Spotify Web API integration for playback state and remote controls.
"""
from typing import Dict, Any, Tuple, Optional
import requests
from spotify_auth import auth_manager
from spotify_models import SpotifyState

SPOTIFY_BASE_URL = "https://api.spotify.com/v1"

class SpotifyApiClient:
    def __init__(self):
        self.session = requests.Session()
        self.last_device_id: Optional[str] = None
        self.last_known_track: Optional[Dict[str, Any]] = None

    def _get_auth_headers(self) -> Optional[Dict[str, str]]:
        token = auth_manager.get_valid_access_token()
        if not token:
            return None
        return {
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        }

    def _request(self, method: str, endpoint: str, **kwargs) -> Tuple[int, Optional[Any]]:
        headers = self._get_auth_headers()
        if not headers:
            return 401, {"error": "Not authenticated. Please visit /login to authorize Spotify."}

        url = f"{SPOTIFY_BASE_URL}{endpoint}"
        kwargs.setdefault("timeout", 5)
        kwargs["headers"] = headers

        try:
            resp = self.session.request(method, url, **kwargs)
            
            # If 401 Unauthorized, try refreshing token once
            if resp.status_code == 401:
                refreshed_token = auth_manager.refresh_access_token()
                if refreshed_token:
                    headers["Authorization"] = f"Bearer {refreshed_token}"
                    kwargs["headers"] = headers
                    resp = self.session.request(method, url, **kwargs)

            if resp.status_code == 204:
                return 204, None

            if resp.status_code >= 400:
                try:
                    err_json = resp.json()
                    err_msg = err_json.get("error", {}).get("message", resp.text)
                except Exception:
                    err_msg = resp.text
                return resp.status_code, {"error": err_msg}

            if resp.text.strip():
                return resp.status_code, resp.json()
            return resp.status_code, None

        except requests.RequestException as e:
            return 503, {"error": f"Network error communicating with Spotify: {str(e)}"}

    def get_devices(self) -> list[Dict[str, Any]]:
        """Returns list of available Spotify Connect devices."""
        status_code, data = self._request("GET", "/me/player/devices")
        if status_code == 200 and isinstance(data, dict):
            return data.get("devices", [])
        return []

    def get_target_device_id(self) -> Optional[str]:
        """Finds the best device to send playback commands to."""
        devices = self.get_devices()
        if not devices:
            return self.last_device_id

        # 1. Prefer currently active device
        for d in devices:
            if d.get("is_active"):
                self.last_device_id = d.get("id")
                return self.last_device_id

        # 2. Prefer previously used device if still present
        if self.last_device_id:
            for d in devices:
                if d.get("id") == self.last_device_id:
                    return self.last_device_id

        # 3. Prefer Computer or Smartphone
        for d in devices:
            dev_type = str(d.get("type", "")).lower()
            if dev_type in ("computer", "smartphone"):
                self.last_device_id = d.get("id")
                return self.last_device_id

        # 4. Fallback to first available device
        if devices:
            self.last_device_id = devices[0].get("id")
            return self.last_device_id

        return self.last_device_id

    def get_state(self) -> Dict[str, Any]:
        """Fetches normalized current playback state with idle caching."""
        status_code, data = self._request("GET", "/me/player")

        if status_code == 401:
            return SpotifyState.error("Authentication required. Visit /login on the bridge.")

        if status_code == 204 or data is None:
            # Player is idle - check if we have cached track info from previous playback
            if self.last_known_track:
                state_dict = dict(self.last_known_track)
                state_dict["playing"] = False
                state_dict["ok"] = True
                return state_dict
            return SpotifyState.idle().to_dict()

        if status_code != 200 or not isinstance(data, dict):
            err_msg = data.get("error") if isinstance(data, dict) else "Unknown Spotify error"
            return SpotifyState.error(str(err_msg))

        # Cache active device
        device_obj = data.get("device")
        if isinstance(device_obj, dict) and device_obj.get("id"):
            self.last_device_id = device_obj.get("id")

        is_playing = bool(data.get("is_playing", False))
        progress_ms = int(data.get("progress_ms") or 0)
        item = data.get("item")

        if not item or not isinstance(item, dict):
            state = SpotifyState(
                ok=True,
                playing=is_playing,
                track_id=None,
                title="Ready to Play",
                artist="Spotify",
                album="",
                duration_ms=0,
                progress_ms=progress_ms,
                artwork_url=None,
            ).to_dict()
            return state

        track_id = item.get("id")
        title = item.get("name", "Unknown Title")
        
        # Artist formatting (join multiple artists)
        artists_list = item.get("artists", [])
        artist_names = [a.get("name", "") for a in artists_list if isinstance(a, dict) and a.get("name")]
        artist = ", ".join(artist_names) if artist_names else "Unknown Artist"

        album_obj = item.get("album", {})
        album_name = album_obj.get("name", "") if isinstance(album_obj, dict) else ""

        # Artwork URL selection
        artwork_url = None
        if isinstance(album_obj, dict):
            images = album_obj.get("images", [])
            if images and isinstance(images, list):
                artwork_url = images[0].get("url") if isinstance(images[0], dict) else None

        duration_ms = int(item.get("duration_ms") or 0)

        current_state = SpotifyState(
            ok=True,
            playing=is_playing,
            track_id=track_id,
            title=title,
            artist=artist,
            album=album_name,
            duration_ms=duration_ms,
            progress_ms=progress_ms,
            artwork_url=artwork_url,
        ).to_dict()

        # Cache last known track
        if track_id:
            self.last_known_track = current_state

        return current_state

    def play(self) -> Dict[str, Any]:
        """Resumes or starts Spotify playback, waking idle devices automatically."""
        # 1. Try standard resume
        status_code, data = self._request("PUT", "/me/player/play")
        if status_code in (200, 204):
            return {"ok": True}

        # 2. If no active device or player was idle, find target device
        target_device = self.get_target_device_id()
        if target_device:
            # Try play with explicit device ID
            status_code, data = self._request("PUT", f"/me/player/play?device_id={target_device}")
            if status_code in (200, 204):
                return {"ok": True}

            # If that fails (e.g. restriction or no track context), transfer playback with play=True
            status_code, data = self._request("PUT", "/me/player", json={"device_ids": [target_device], "play": True})
            if status_code in (200, 204):
                return {"ok": True}

            # If still needed and we have a cached track URI, start track explicitly
            if self.last_known_track and self.last_known_track.get("track_id"):
                tid = self.last_known_track.get("track_id")
                status_code, data = self._request(
                    "PUT",
                    f"/me/player/play?device_id={target_device}",
                    json={"uris": [f"spotify:track:{tid}"]}
                )
                if status_code in (200, 204):
                    return {"ok": True}

        err = data.get("error", "No active Spotify device found. Please open Spotify.") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def pause(self) -> Dict[str, Any]:
        """Pauses Spotify playback."""
        status_code, data = self._request("PUT", "/me/player/pause")
        if status_code in (200, 204):
            return {"ok": True}

        target_device = self.get_target_device_id()
        if target_device:
            status_code, data = self._request("PUT", f"/me/player/pause?device_id={target_device}")
            if status_code in (200, 204):
                return {"ok": True}

        err = data.get("error", "Failed to pause playback") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def toggle_play_pause(self) -> Dict[str, Any]:
        """Toggles between play and pause based on current state."""
        state = self.get_state()
        if not state.get("ok"):
            # If state check failed, attempt play anyway
            return self.play()

        if state.get("playing"):
            return self.pause()
        else:
            return self.play()

    def next_track(self) -> Dict[str, Any]:
        """Skips to the next track, waking device if needed."""
        status_code, data = self._request("POST", "/me/player/next")
        if status_code in (200, 204):
            return {"ok": True}

        target_device = self.get_target_device_id()
        if target_device:
            status_code, data = self._request("POST", f"/me/player/next?device_id={target_device}")
            if status_code in (200, 204):
                return {"ok": True}

        err = data.get("error", "Failed to skip to next track") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def previous_track(self) -> Dict[str, Any]:
        """Skips to the previous track, waking device if needed."""
        status_code, data = self._request("POST", "/me/player/previous")
        if status_code in (200, 204):
            return {"ok": True}

        target_device = self.get_target_device_id()
        if target_device:
            status_code, data = self._request("POST", f"/me/player/previous?device_id={target_device}")
            if status_code in (200, 204):
                return {"ok": True}

        err = data.get("error", "Failed to skip to previous track") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

# Global singleton API client
spotify_client = SpotifyApiClient()

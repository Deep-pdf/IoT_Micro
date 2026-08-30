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

    def get_state(self) -> Dict[str, Any]:
        """Fetches normalized current playback state."""
        status_code, data = self._request("GET", "/me/player")

        if status_code == 401:
            return SpotifyState.error("Authentication required. Visit /login on the bridge.")

        if status_code == 204 or data is None:
            # Player is idle (no active device or playback)
            return SpotifyState.idle().to_dict()

        if status_code != 200 or not isinstance(data, dict):
            err_msg = data.get("error") if isinstance(data, dict) else "Unknown Spotify error"
            return SpotifyState.error(str(err_msg))

        is_playing = bool(data.get("is_playing", False))
        progress_ms = int(data.get("progress_ms") or 0)
        item = data.get("item")

        if not item or not isinstance(item, dict):
            # E.g. playing a podcast episode or ad without track item
            return SpotifyState(
                ok=True,
                playing=is_playing,
                track_id=None,
                title="Unknown Track",
                artist="",
                album="",
                duration_ms=0,
                progress_ms=progress_ms,
                artwork_url=None,
            ).to_dict()

        track_id = item.get("id")
        title = item.get("name", "Unknown Title")
        
        # Artist formatting (join multiple artists)
        artists_list = item.get("artists", [])
        artist_names = [a.get("name", "") for a in artists_list if isinstance(a, dict) and a.get("name")]
        artist = ", ".join(artist_names) if artist_names else "Unknown Artist"

        album_obj = item.get("album", {})
        album_name = album_obj.get("name", "") if isinstance(album_obj, dict) else ""

        # Artwork URL selection (prefer medium 300x300 or first available)
        artwork_url = None
        if isinstance(album_obj, dict):
            images = album_obj.get("images", [])
            if images and isinstance(images, list):
                # Look for medium size image or fallback to first
                artwork_url = images[0].get("url") if isinstance(images[0], dict) else None

        duration_ms = int(item.get("duration_ms") or 0)

        return SpotifyState(
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

    def play(self) -> Dict[str, Any]:
        """Resumes Spotify playback."""
        status_code, data = self._request("PUT", "/me/player/play")
        if status_code in (200, 204):
            return {"ok": True}
        err = data.get("error", "Failed to start playback") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def pause(self) -> Dict[str, Any]:
        """Pauses Spotify playback."""
        status_code, data = self._request("PUT", "/me/player/pause")
        if status_code in (200, 204):
            return {"ok": True}
        err = data.get("error", "Failed to pause playback") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def toggle_play_pause(self) -> Dict[str, Any]:
        """Toggles between play and pause based on current state."""
        state = self.get_state()
        if not state.get("ok"):
            return {"ok": False, "error": state.get("error", "Cannot determine playback state")}

        if state.get("playing"):
            return self.pause()
        else:
            return self.play()

    def next_track(self) -> Dict[str, Any]:
        """Skips to the next track."""
        status_code, data = self._request("POST", "/me/player/next")
        if status_code in (200, 204):
            return {"ok": True}
        err = data.get("error", "Failed to skip to next track") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

    def previous_track(self) -> Dict[str, Any]:
        """Skips to the previous track."""
        status_code, data = self._request("POST", "/me/player/previous")
        if status_code in (200, 204):
            return {"ok": True}
        err = data.get("error", "Failed to skip to previous track") if isinstance(data, dict) else "Failed"
        return {"ok": False, "error": str(err)}

# Global singleton API client
spotify_client = SpotifyApiClient()

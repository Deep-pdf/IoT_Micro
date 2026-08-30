"""
Spotify OAuth 2.0 Authorization Code flow and token manager.
"""
import os
import json
import time
import secrets
import urllib.parse
import base64
import requests
from typing import Optional, Dict, Any

SPOTIFY_AUTH_URL = "https://accounts.spotify.com/authorize"
SPOTIFY_TOKEN_URL = "https://accounts.spotify.com/api/token"
REQUIRED_SCOPES = [
    "user-read-currently-playing",
    "user-read-playback-state",
    "user-modify-playback-state",
]

class SpotifyAuthManager:
    def __init__(self, token_file: str = "tokens.json"):
        self.token_file = os.path.abspath(os.path.join(os.path.dirname(__file__), token_file))
        self._state_cache: set[str] = set()

    @property
    def client_id(self) -> str:
        return os.getenv("SPOTIFY_CLIENT_ID", "").strip()

    @property
    def client_secret(self) -> str:
        return os.getenv("SPOTIFY_CLIENT_SECRET", "").strip()

    @property
    def redirect_uri(self) -> str:
        return os.getenv("SPOTIFY_REDIRECT_URI", "http://127.0.0.1:8888/callback").strip()

    def is_configured(self) -> bool:
        return bool(self.client_id and self.client_secret and self.redirect_uri)

    def generate_auth_url(self) -> str:
        """Generates the Spotify authorization URL with a secure random state."""
        state = secrets.token_urlsafe(16)
        self._state_cache.add(state)

        params = {
            "client_id": self.client_id,
            "response_type": "code",
            "redirect_uri": self.redirect_uri,
            "scope": " ".join(REQUIRED_SCOPES),
            "state": state,
            "show_dialog": "true",
        }
        return f"{SPOTIFY_AUTH_URL}?{urllib.parse.urlencode(params)}"

    def validate_state(self, state: str) -> bool:
        if state in self._state_cache:
            self._state_cache.remove(state)
            return True
        return False

    def exchange_code_for_token(self, code: str) -> Dict[str, Any]:
        """Exchanges authorization code for access and refresh tokens."""
        if not self.is_configured():
            raise ValueError("Spotify credentials not configured in environment.")

        auth_header = base64.b64encode(f"{self.client_id}:{self.client_secret}".encode()).decode()
        headers = {
            "Authorization": f"Basic {auth_header}",
            "Content-Type": "application/x-www-form-urlencoded",
        }
        data = {
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": self.redirect_uri,
        }

        response = requests.post(SPOTIFY_TOKEN_URL, headers=headers, data=data, timeout=10)
        if response.status_code != 200:
            raise RuntimeError(f"Spotify token exchange failed ({response.status_code}): {response.text}")

        token_data = response.json()
        self._save_tokens(token_data)
        return token_data

    def refresh_access_token(self) -> Optional[str]:
        """Refreshes the access token using the stored refresh token."""
        stored = self._load_tokens()
        refresh_token = stored.get("refresh_token")
        if not refresh_token:
            return None

        auth_header = base64.b64encode(f"{self.client_id}:{self.client_secret}".encode()).decode()
        headers = {
            "Authorization": f"Basic {auth_header}",
            "Content-Type": "application/x-www-form-urlencoded",
        }
        data = {
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
        }

        try:
            response = requests.post(SPOTIFY_TOKEN_URL, headers=headers, data=data, timeout=10)
            if response.status_code != 200:
                print(f"[SpotifyAuth] Token refresh error ({response.status_code}): {response.text}")
                return None

            token_data = response.json()
            # If a new refresh_token was not returned, keep the existing one
            if "refresh_token" not in token_data:
                token_data["refresh_token"] = refresh_token

            self._save_tokens(token_data)
            return token_data.get("access_token")
        except Exception as e:
            print(f"[SpotifyAuth] Exception while refreshing token: {e}")
            return None

    def get_valid_access_token(self) -> Optional[str]:
        """Returns a valid access token, automatically refreshing if expired."""
        stored = self._load_tokens()
        access_token = stored.get("access_token")
        expires_at = stored.get("expires_at", 0)

        if not access_token:
            return None

        # Refresh if token will expire within 60 seconds
        if time.time() >= (expires_at - 60):
            return self.refresh_access_token()

        return access_token

    def is_authenticated(self) -> bool:
        return self.get_valid_access_token() is not None

    def _save_tokens(self, token_data: Dict[str, Any]) -> None:
        expires_in = token_data.get("expires_in", 3600)
        payload = {
            "access_token": token_data.get("access_token"),
            "refresh_token": token_data.get("refresh_token"),
            "token_type": token_data.get("token_type", "Bearer"),
            "scope": token_data.get("scope", ""),
            "expires_at": int(time.time() + expires_in),
        }
        with open(self.token_file, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)

    def _load_tokens(self) -> Dict[str, Any]:
        if not os.path.exists(self.token_file):
            return {}
        try:
            with open(self.token_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"[SpotifyAuth] Error reading {self.token_file}: {e}")
            return {}

# Global singleton auth manager
auth_manager = SpotifyAuthManager()

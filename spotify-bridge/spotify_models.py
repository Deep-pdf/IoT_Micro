"""
Data models and response helpers for Spotify state and action payloads.
"""
from dataclasses import dataclass, asdict
from typing import Optional, Dict, Any

@dataclass
class SpotifyState:
    ok: bool = True
    playing: bool = False
    track_id: Optional[str] = None
    title: str = ""
    artist: str = ""
    album: str = ""
    duration_ms: int = 0
    progress_ms: int = 0
    artwork_url: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    @classmethod
    def idle(cls) -> "SpotifyState":
        return cls(
            ok=True,
            playing=False,
            track_id=None,
            title="",
            artist="",
            album="",
            duration_ms=0,
            progress_ms=0,
            artwork_url=None,
        )

    @classmethod
    def error(cls, error_message: str) -> Dict[str, Any]:
        return {
            "ok": False,
            "error": error_message,
            "playing": False,
            "track_id": None,
            "title": "",
            "artist": "",
            "album": "",
            "duration_ms": 0,
            "progress_ms": 0,
            "artwork_url": None,
        }

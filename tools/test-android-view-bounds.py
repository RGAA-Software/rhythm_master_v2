"""Guard native hierarchy clipping detection; no device or timing assumptions."""
import unittest
from android_view_bounds import parse_views, require_unclipped, require_player_focus, reject_call_ui

HEADER = "ACTIVITY org.rhythmmaster.player/.PlayerActivity\n    View Hierarchy:\n"


class BoundsTests(unittest.TestCase):
    def test_parent_clipping_and_visibility(self):
        text = HEADER + """      android.widget.FrameLayout{a V.E...... ........ 20,80-1100,2300}
        android.widget.ScrollView{b VFED.V... ........ 0,1400-1080,2000}
          android.widget.LinearLayout{c V.E...... ........ 0,0-1080,900}
            android.widget.TextView{d V.ED..... ........ 0,580-1080,640 #123 app:id/player_music_time}
"""
        views = parse_views(text)
        self.assertEqual(views["player_music_time"]["bounds"], [20, 2060, 1100, 2120])
        self.assertEqual(views["player_music_time"]["visible_bounds"], [20, 2060, 1100, 2080])
        with self.assertRaisesRegex(ValueError, "clipped"):
            require_unclipped(views, "player_music_time")
        repaired = parse_views(text.replace("1400-1080,2000", "1400-1080,2200"))
        self.assertEqual(require_unclipped(repaired, "player_music_time"), [20, 2060, 1100, 2120])
        hidden = parse_views(text.replace("b VFED", "b GFED"))
        with self.assertRaisesRegex(ValueError, "clipped"):
            require_unclipped(hidden, "player_music_time")

    def test_external_windows_block_device_input(self):
        for focus in (b"mCurrentFocus=null", b"mCurrentFocus=Window{a com.android.incallui/.Call}"):
            with self.assertRaisesRegex(RuntimeError, "foreground"):
                require_player_focus(lambda *args: focus)
        with self.assertRaisesRegex(RuntimeError, "Call UI"):
            reject_call_ui(lambda *args: b"mCurrentFocus=Window{a com.android.incallui/.Call}")
        require_player_focus(lambda *args: b"mCurrentFocus=Window{a org.rhythmmaster.player/.PlayerActivity}")

    def test_stale_or_unrelated_activity_rejected(self):
        with self.assertRaises(ValueError):
            parse_views("ACTIVITY another.app/.Activity\n    View Hierarchy:\n")
        with self.assertRaises(ValueError):
            parse_views(HEADER.replace("View Hierarchy:", "window unavailable"))

    def test_top_focused_display_controls_input(self):
        dump = b"""Display: mDisplayId=420
  mCurrentFocus=null
Display: mDisplayId=0 (organized)
  mCurrentFocus=Window{a org.rhythmmaster.player/.PlayerActivity}
mTopFocusedDisplayId=0
"""
        require_player_focus(lambda *args: dump)
        with self.assertRaisesRegex(RuntimeError, "foreground"):
            require_player_focus(lambda *args: dump.replace(b"mTopFocusedDisplayId=0", b"mTopFocusedDisplayId=420"))
        with self.assertRaisesRegex(RuntimeError, "foreground"):
            require_player_focus(lambda *args: dump.replace(b"mTopFocusedDisplayId=0", b""))


if __name__ == "__main__":
    unittest.main()

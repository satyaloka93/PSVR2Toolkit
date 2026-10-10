"""CET/Native Settings behavior tests; no game or controller required (lupa)."""
from pathlib import Path
import unittest
from lupa import LuaRuntime

SOURCE = Path(__file__).resolve().parents[1] / 'package/cet/PSVR2 Haptics/init.lua'


class AudioMenuTests(unittest.TestCase):
    def create(self, saved=None, tab=True, dependency=True):
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute('''
            events = {}; files = {}; options = {}; paths = {}; writes = 0; categories = 0
            failWrite = false; dependency = true
            function registerForEvent(name, callback) events[name] = callback end
            io.open = function(path, mode)
                assert(path == "settings.ini", "must never edit EDS files")
                if mode == "r" then
                    if not files[path] then return nil end
                    return { read = function() return files[path] end, close = function() return true end }
                end
                if failWrite then return nil, "read only" end
                return {write = function(self, data) files[path] = data; writes = writes + 1; return self end,
                        close = function() return true end}
            end
            ns = {}
            ns.pathExists = function(path) return paths[path] == true end
            ns.addSubcategory = function(path, label) assert(paths['/DS_ATE']); paths[path] = true; categories = categories + 1 end
            ns.addTab = function() error("must not replace the existing EDS tab") end
            ns.addSwitch = function(path, label, desc, value, default, callback)
                options.toggle = {state=value, default=default, callback=callback}; return options.toggle
            end
            ns.addRangeInt = function(path, label, desc, min, max, step, value, default, callback)
                options.slider = {state=value, default=default, callback=callback}; return options.slider
            end
            ns.setOption = function(option, value)
                if option.state == value then return end
                option.state = value; option.callback(value)
            end
            ns.removeSubcategory = function(path) paths[path] = nil end
            GetMod = function(name) assert(name == 'nativeSettings'); if dependency then return ns end end
        ''')
        lua.globals().paths['/DS_ATE'] = tab
        lua.globals().dependency = dependency
        if saved is not None:
            lua.globals().files['settings.ini'] = saved
        lua.execute(SOURCE.read_text())
        lua.globals().events.onInit()
        return lua

    def test_defaults_and_existing_tab(self):
        lua = self.create()
        self.assertFalse(lua.globals().options.toggle.state)
        self.assertEqual(lua.globals().options.slider.state, 35)
        self.assertEqual(lua.globals().files['settings.ini'], 'Version=1\nAudioEnabled=0\nAudioStrength=35\n')
        for _ in range(5):
            lua.globals().events.onUpdate(1)
        self.assertEqual(lua.globals().categories, 1)

    def test_live_saved_controls_and_restart(self):
        lua = self.create()
        lua.globals().ns.setOption(lua.globals().options.toggle, True)
        lua.globals().ns.setOption(lua.globals().options.slider, 80)
        text = lua.globals().files['settings.ini']
        self.assertEqual(text, 'Version=1\nAudioEnabled=1\nAudioStrength=80\n')
        other = self.create(saved=text)
        self.assertTrue(other.globals().options.toggle.state)
        self.assertEqual(other.globals().options.slider.state, 80)
        self.assertEqual(other.globals().writes, 0)

    def test_delayed_dependencies_and_tab_rebuild(self):
        lua = self.create(tab=False, dependency=False)
        self.assertEqual(lua.globals().categories, 0)
        lua.globals().dependency = True
        lua.globals().events.onUpdate(1)
        self.assertEqual(lua.globals().categories, 0)
        lua.globals().paths['/DS_ATE'] = True
        lua.globals().events.onUpdate(1)
        self.assertEqual(lua.globals().categories, 1)
        lua.globals().paths['/DS_ATE/psvr2Haptics'] = False
        lua.globals().events.onUpdate(1)
        self.assertEqual(lua.globals().categories, 2)

    def test_bad_or_partial_file_defaults_off_without_overwrite(self):
        for saved in ('', 'Version=1\nAudioEnabled=1\n', 'Version=2\nAudioEnabled=1\nAudioStrength=35\n',
                      'Version=1\nAudioEnabled=1\nAudioStrength=301\n',
                      'Version=1\nAudioEnabled=1\nAudioStrength=35\nAudioEnabled=0\n'):
            lua = self.create(saved=saved)
            self.assertFalse(lua.globals().options.toggle.state)
            self.assertEqual(lua.globals().writes, 0)

    def test_off_preserves_strength(self):
        lua = self.create(saved='Version=1\r\nAudioEnabled=1\r\nAudioStrength=135\r\n')
        lua.globals().ns.setOption(lua.globals().options.toggle, False)
        self.assertEqual(lua.globals().files['settings.ini'], 'Version=1\nAudioEnabled=0\nAudioStrength=135\n')

    def test_save_failure_rolls_back_visible_control(self):
        lua = self.create()
        lua.globals().failWrite = True
        lua.globals().ns.setOption(lua.globals().options.toggle, True)
        lua.globals().ns.setOption(lua.globals().options.slider, 200)
        self.assertFalse(lua.globals().options.toggle.state)
        self.assertEqual(lua.globals().options.slider.state, 35)

    def test_shutdown_removes_only_own_category(self):
        lua = self.create()
        lua.globals().events.onShutdown()
        self.assertTrue(lua.globals().paths['/DS_ATE'])
        self.assertIsNone(lua.globals().paths['/DS_ATE/psvr2Haptics'])


if __name__ == '__main__':
    unittest.main()

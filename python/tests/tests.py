import unittest

from vesna.spectrumsensor import Device, DeviceConfig, SweepConfig, DeviceConfig, ConfigList

class TestDeviceConfig(unittest.TestCase):
	def setUp(self):
		self.d = Device(0, "test")

		self.dc = DeviceConfig(0, "test", self.d)
		self.dc.base = 1000
		self.dc.spacing = 30
		self.dc.num = 1000

	def test_get_full_sweep_config_1(self):
		sc = self.dc.get_full_sweep_config()
		self.assertEquals(sc.step_ch, 1)

	def test_get_full_sweep_config_2(self):
		sc = self.dc.get_full_sweep_config(step_hz=5)
		self.assertEquals(sc.step_ch, 1)

	def test_get_full_sweep_config_3(self):
		sc = self.dc.get_full_sweep_config(step_hz=35)
		self.assertEquals(sc.step_ch, 1)

	def test_get_full_sweep_config_3(self):
		sc = self.dc.get_full_sweep_config(step_hz=45)
		self.assertEquals(sc.step_ch, 2)

class TestSweepConfig(unittest.TestCase):
	def setUp(self):
		self.d = Device(0, "test")

		self.dc = DeviceConfig(0, "test", self.d)
		self.dc.base = 1000
		self.dc.spacing = 1
		self.dc.num = 1000

	def test_stop_hz_1(self):
		# 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
		sc = SweepConfig(self.dc, start_ch=0, stop_ch=10, step_ch=1)

		self.assertEquals(sc.stop_ch, 10)
		self.assertEquals(sc.stop_hz, 1009)

	def test_stop_hz_2(self):
		# 0, 13, 26, 39, 52
		sc = SweepConfig(self.dc, start_ch=0, stop_ch=50, step_ch=13)

		self.assertEquals(sc.stop_ch, 50)
		self.assertEquals(sc.stop_hz, 1039)

class TestConfigList(unittest.TestCase):
	def test_get_config_name(self):
		cl = ConfigList()

		d = Device(0, "test")
		cl._add_device(d)

		def add_dc(id, name, base):
			dc = DeviceConfig(id, name, d)
			dc.base = base
			dc.spacing = 1
			dc.num = 1000
			dc.time = 1
			cl._add_config(dc)

		add_dc(0, "foo 1", 1000)
		add_dc(1, "foo 2", 2000)
		add_dc(2, "bar 1", 1000)
		add_dc(3, "bar 2", 2000)

		sc = cl.get_sweep_config(1500, 1600, 1)
		self.assertEquals(0, sc.config.id)

		sc = cl.get_sweep_config(2500, 2600, 1)
		self.assertEquals(1, sc.config.id)

		sc = cl.get_sweep_config(1500, 1600, 1, name="bar")
		self.assertEquals(2, sc.config.id)

		sc = cl.get_sweep_config(2500, 2600, 1, name="bar")
		self.assertEquals(3, sc.config.id)

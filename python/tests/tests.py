import unittest

from vesna.spectrumsensor import Device, DeviceConfig, SweepConfig, DeviceConfig

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

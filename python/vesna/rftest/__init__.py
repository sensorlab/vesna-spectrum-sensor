import optparse
import os

class usbtmc:
	def __init__(self, device):
		self.device = device
		self.f = os.open(device, os.O_RDWR)

	def write(self, command):
		os.write(self.f, command);

	def read(self, length=4000):
		return os.read(self.f, length)

	def query(self, command, length=300):
		self.write(command)
		return self.read(length)

	def get_name(self):
		return self.query("*IDN?")

	def send_reset(self):
		self.write("*RST")

	def close(self):
		os.close(self.f)

class SignalGenerator(usbtmc):
	def rf_on(self, freq_hz, power_dbm):

		power_dbm = max(-145, power_dbm)

		self.write("freq %d Hz\n" % (freq_hz,))
		self.write("pow %d dBm\n" % (power_dbm,))
		self.write("outp on\n")

	def rf_off(self):
		self.write("outp off\n")

class DeviceUnderTest:
	def __init__(self, args, name, device_id=0, config_id=0, replay=False, log_path=None):
		self.name = name
		self.device_id = device_id
		self.config_id = config_id
		self._replay = replay
		self.log_path = log_path

		self._extra = 150

		options = self._optparse(args)
		self.setup(options)

	def _optparse(self, args):

		usage= "usage: %%prog -R%s -O,[--option=value,...]" % (self.__class__,)

		parser = optparse.OptionParser(usage=usage)
		self.add_options(parser)


		args = args.strip(',').split(',')
		(options, args) = parser.parse_args(args)

		return options

	def add_options(self, parser):
		pass

	def setup(self, options):
		pass

	def is_replay(self):
		return self._replay

	def get_fw_version(self):
		return ""

	def get_status(self):
		return [""]

	def measure_ch(self, ch, n, name):
		if self._replay:
			return self._measure_ch_replay(name)
		else:
			return self._measure_ch_real(ch, n, name)

	def _measure_ch_real(self, ch, n, name):
		assert ch < self.config.num

		measurements = self.measure_ch_impl(ch, n + self._extra)
		measurements = measurements[self._extra:]

		self._measure_ch_save(name, measurements)
		return measurements

	def measure_ch_impl(self, ch, n):
		return [0.0] * n

	def _measure_ch_save(self, name, measurements):
		if self.log_path:
			path = ("%s/%s_%s.log" % (self.log_path, self.name, name)).replace("-", "m")
			f = open(path, "w")
			f.write("# P [dBm]\n")
			f.write("\n".join(map(str, measurements)))
			f.close()

	def _measure_ch_replay(self, name):
		path = ("%s/%s_%s.log" % (self.log_path, self.name, name)).replace("-", "m")
		f = open(path)

		return map(float, filter(lambda x:not x.startswith("#"), f))

import re
import select
import serial

class Device:
	def __init__(self, id, name):
		self.id = id
		self.name = name

class DeviceConfig: 
	def __init__(self, id, name, device):
		self.id = id
		self.name = name
		self.device = device

	def ch_to_hz(self, ch):
		return self.base + self.spacing * ch

	def get_start_hz(self):
		return self.ch_to_hz(0)

	def get_stop_hz(self):
		return self.ch_to_hz(self.num - 1)

	def covers(self, start_hz, stop_hz):
		"""Return true if this configuration can cover the given band
		"""
		return start_hz >= self.get_start_hz() and stop_hz <= self.get_stop_hz()

	def get_full_sweep_config(self, step_hz=None):
		if step_hz is None:
			step_ch = 1
		else:
			step_ch = max(1, int(round(float(step_hz) / self.spacing)))

		return SweepConfig(self, 0, self.num, step_ch)

	def get_sweep_config(self, start_hz, stop_hz, step_hz):
		assert self.covers(start_hz, stop_hz)

		# channel start, step, stop as passed to vesna channel config
		# (stop_ch is one past the last channel to be scanned)
		start_ch = int(round((start_hz - self.base) / self.spacing))
		step_ch = max(1, int(round(step_hz / self.spacing)))
		stop_ch = int(round((stop_hz - self.base) / self.spacing)) + 1

		return SweepConfig(self, start_ch, stop_ch, step_ch)

	def __str__(self):
		return "channel config %d,%d: %10d - %10d Hz" % (
				self.device.id, self.id, self.get_start_hz(), self.get_stop_hz())

class SweepConfig:
	def __init__(self, config, start_ch, stop_ch, step_ch):
		assert start_ch >= 0
		assert start_ch < config.num
		assert stop_ch >= 0
		assert stop_ch <= config.num
		assert step_ch > 0

		self.config = config
		self.start_ch = start_ch
		self.stop_ch = stop_ch
		self.step_ch = step_ch

		# given (start_ch - stop_ch) range may not be an integer number of step_ch
		last_ch = stop_ch - (stop_ch - start_ch - 1) % step_ch - 1

		# real frequency start, step, stop
		# (stop_hz is the frequency of the last channel)
		self.start_hz = config.ch_to_hz(start_ch)
		self.stop_hz = config.ch_to_hz(last_ch)
		self.step_hz = config.spacing * step_ch

		self.num_channels = len(range(start_ch, stop_ch, step_ch))

class Sweep:
	def __init__(self):
		self.timestamp = None
		self.data = []

class ConfigList:
	def __init__(self):
		self.configs = []
		self.devices = []

	def _add_device(self, device):
		self.devices.append(device)

	def _add_config(self, config):
		self.configs.append(config)

	def get_config(self, device_id, config_id):
		for config in self.configs:
			if config.id == config_id and config.device.id == device_id:
				return config

		return None

	def get_sweep_config(self, start_hz, stop_hz, step_hz):

		candidates = []

		for config in self.configs:
			if config.covers(start_hz, stop_hz):
				candidates.append(config)

		# pick fastest matching config
		candidates.sort(key=lambda x:x.time, reverse=True)

		if candidates:
			return candidates[0].get_sweep_config(start_hz, stop_hz, step_hz)
		else:
			return None

class SpectrumSensor:
	def __init__(self, device):
		self.comm = serial.Serial(device, 115200, timeout=.5)
	
	def get_config_list(self):
		self.comm.write("report-off\n")

		while self.comm.readline() != 'ok\n': pass

		self.comm.write("list\n")

		config_list = ConfigList()

		device = None
		config = None
		while True:
			line = self.comm.readline()
			if not line:
				break

			g = re.match("device ([0-9]+): (.*)", line)
			if g:
				device = Device(int(g.group(1)), g.group(2))
				config_list._add_device(device)
				continue

			g = re.match("  channel config ([0-9]+),([0-9]+): (.*)", line)
			if g:
				config = DeviceConfig(int(g.group(2)), g.group(3), device)
				config_list._add_config(config)
				continue

			g = re.match("    ([a-z]+): ([0-9]+)", line)
			if g:
				value = int(g.group(2))
				assert value >= 0
				setattr(config, g.group(1), value)
				continue

		return config_list

	def get_status(self):
		self.comm.write("status\n")

		resp = []

		while True:
			line = self.comm.readline()
			if line:
				resp.append(line)
			else:
				break

		return resp

	def run(self, sweep_config, cb):
		self.comm.write("select channel %d:%d:%d config %d,%d\n" % (
				sweep_config.start_ch, sweep_config.step_ch, sweep_config.stop_ch,
				sweep_config.config.device.id, sweep_config.config.id))

		while self.comm.readline() != 'ok\n': pass

		self.comm.write("report-on\n")

		self.comm.timeout = None

		while True:
			try:
				line = self.comm.readline()
			except select.error:
				break

			if not line:
				break

			try:
				fields = line.split()
				if len(fields) != sweep_config.num_channels + 4:
					raise ValueError

				sweep = Sweep()

				sweep.timestamp = float(fields[1])
				sweep.data = map(float, fields[3:-1])
			except ValueError:
				print "Ignoring corrupted line: %s" % (line,)
				continue

			if not cb(sweep_config, sweep):
				break

		self.comm.timeout = 0.5

		self.comm.write("report-off\n")

		while self.comm.readline() != 'ok\n': pass

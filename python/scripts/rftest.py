import datetime
import optparse
import os
import numpy
import math
import scipy.integrate
import sys
import time
from vesna.spectrumsensor import SpectrumSensor, SweepConfig

log_f = None

def log(msg):
	log_f.write(msg + "\n")
	print msg

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
	def __init__(self, device, name, replay=False, log_path=None):
		self.name = name
		self.replay = replay
		self.log_path = log_path

		self.extra = 150

		self.spectrumsensor = SpectrumSensor(device)

		self.config_list = self.spectrumsensor.get_config_list()
		if not self.config_list.configs:
			raise Exception("Device returned no configurations. "
					"It is still scanning or not responding.")

		config_id = 0
		device_id = 0

		self.config = self.config_list.get_config(config_id, device_id)

	def measure_ch(self, ch, n, name):
		if self.replay:
			return self._measure_ch_replay(name)
		else:
			return self._measure_ch_real(ch, n, name)

	def _measure_ch_real(self, ch, n, name):
		assert ch < self.config.num

		sweep_config = SweepConfig(self.config, ch, ch+1, 1)

		measurements = []

		def cb(sweep_config, sweep):
			assert len(sweep.data) == 1
			measurements.append(sweep.data[0])
			return len(measurements) < (n + self.extra)

		self.spectrumsensor.run(sweep_config, cb)
		measurements = measurements[self.extra:]

		self._measure_ch_save(name, measurements)
		return measurements

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

def chop(p_dbm_list, pout_dbm_list, min_dbm, max_dbm):
	p_dbm_list2 = []
	pout_dbm_list2 = []
	for p_dbm, pout_dbm in zip(p_dbm_list, pout_dbm_list):
		if p_dbm >= min_dbm and p_dbm <= max_dbm:
			p_dbm_list2.append(p_dbm)
			pout_dbm_list2.append(pout_dbm)

	return p_dbm_list2, pout_dbm_list2

def max_error(pin_dbm_list, pout_dbm_list):
	return max(	abs(i - o)
			for i, o in zip(pin_dbm_list, pout_dbm_list) )

def get_linear_range(k, n, pin_dbm_list, pout_dbm_list):

	emax = -1
	for i, (pin_dbm, pout_dbm) in enumerate(zip(pin_dbm_list, pout_dbm_list)):

		pout_dbm_lin = pin_dbm * k + n

		e = abs(pout_dbm - pout_dbm_lin)

		if pin_dbm < -40:
			emax = max(emax, e)
		else:
			if e > emax * 1.5:
				break

	log("      saturation @ Pin = %.1f dBm" % (pin_dbm_list[i-1],))

def test_power_ramp(dut, gen):
	"""Check measurement errors versus input power
	"""

	log("Start power ramp test")

	N = 100

	nruns = 3
	ch_num = dut.config.num
	ch_list = [ int(ch_num*(i+0.5)/nruns) for i in xrange(nruns) ]

	for ch in ch_list:
		f_hz = dut.config.ch_to_hz(ch)

		log("  f = %d Hz" % (f_hz))
		
		gen.rf_off()

		nf = dut.measure_ch(ch, N, "power_ramp_%dhz_off" % (f_hz,))
		nf_mean = numpy.mean(nf)

		log("    N = %f dBm, u = %f" % (nf_mean, numpy.std(nf)))

		p_dbm_start = int(round(nf_mean/10)*10-30)
		p_dbm_step = 5

		p_dbm_list = range(p_dbm_start, p_dbm_step, p_dbm_step)
		pout_dbm_list = []
		for p_dbm in p_dbm_list:
			log("    Pin = %d dBm" % (p_dbm))
			gen.rf_on(f_hz, p_dbm)
			s = dut.measure_ch(ch, N, "power_ramp_%dhz_%ddbm" % (f_hz, p_dbm))
			s_mean = numpy.mean(s)
			log("       Pout = %f dBm, u = %f" % (s_mean, numpy.std(s)))
			pout_dbm_list.append(s_mean)

		gen.rf_off()


		f = open("log/%s_power_ramp_%dhz.log" % (dut.name, f_hz,), "w")
		f.write("# Pin [dBm]\tPout [dBm]\n")
		for p_dbm, pout_dbm in zip(p_dbm_list, pout_dbm_list):
			f.write("%f\t%f\n" % (p_dbm, pout_dbm))
		f.close()

		r, m = chop(p_dbm_list, pout_dbm_list, nf_mean+20, 0)
		log("    Range %.1f - %.1f dBm" % (r[0], r[-1]))
		log("      max absolute error %.1f dBm" % (max_error(r, m)))

		r, m = chop(p_dbm_list, pout_dbm_list, nf_mean+20, -40)
		log("    Range %.1f - %.1f dBm" % (r[0], r[-1]))
		log("      max absolute error %.1f dBm" % (max_error(r, m)))

		A = numpy.array([numpy.ones(len(r))])
		n = numpy.linalg.lstsq(A.T, numpy.array(m) - numpy.array(r))[0][0]
		log("      offset = %f dBm" % (n,))

		A = numpy.array([r, numpy.ones(len(r))])
		k, n = numpy.linalg.lstsq(A.T, m)[0]
		log("      linear regression: k = %f, n = %f dBm" % (k, n))

		r, m = chop(p_dbm_list, pout_dbm_list, nf_mean+20, 0)
		get_linear_range(k, n, r, m)

	log("End power ramp test")

def test_freq_sweep(dut, gen):
	"""Check measurement errors versus tuned frequency
	"""

	log("Start frequency sweep test")

	N = 100

	p_dbm_list = [ -90, -75, -60, -45 ]
	for p_dbm in p_dbm_list:

		log("  Pin = %d dBm" % (p_dbm,))

		nruns = 50
		ch_num = dut.config.num
		ch_list = [ int((ch_num-1)*i/(nruns-1)) for i in xrange(nruns) ]

		f_hz_list = []
		pout_dbm_list = []
		for ch in ch_list:
			f_hz = dut.config.ch_to_hz(ch)
			f_hz_list.append(f_hz)

			log("    f = %d Hz" % (f_hz))
			gen.rf_on(f_hz, p_dbm)
			s = dut.measure_ch(ch, N, "freq_sweep_%ddbm_%dhz" % (p_dbm, f_hz))
			s_mean = numpy.mean(s)
			log("       Pout = %f dBm, u = %f" % (s_mean, numpy.std(s)))
			pout_dbm_list.append(s_mean)

		gen.rf_off()

		path = ("log/%s_freq_sweep_%ddbm.log" % (dut.name, p_dbm)).replace("-", "m")
		f = open(path, "w")
		f.write("# f [Hz]\tPout [dBm]\n")
		for f_hz, pout_dbm in zip(f_hz_list, pout_dbm_list):
			f.write("%f\t%f\n" % (f_hz, pout_dbm))
		f.close()

		log("    Range %.1f - %.1f Hz" % (f_hz_list[0], f_hz_list[-1]))
		log("      max absolute error %.1f dBm" % (max_error([p_dbm]*len(f_hz_list), pout_dbm_list)))

	log("End power ramp test")

def get_settle_time(measurements, settled):
	mmax = max(settled)
	mmin = min(settled)

	for n, v in enumerate(measurements):
		if v <= mmax and v >= mmin:
			return n

def test_settle_time(dut, gen):
	"""Measure automatic gain control settling time"""
	
	log("Start settle time test")

	N = 1000

	nruns = 3
	ch_num = dut.config.num
	ch_list = [ int(ch_num*(i+0.5)/nruns) for i in xrange(nruns) ]

	p_dbm_list = [ -90, -50, -10 ]

	for p_dbm in p_dbm_list:
		log("  Pin = %d dBm" % (p_dbm,))

		for ch in ch_list:
			f_hz = dut.config.ch_to_hz(ch)

			log("    f = %d Hz" % (f_hz,))

			step = 200

			gen.rf_off()

			sweep_config = SweepConfig(dut.config, ch, ch+1, 1)

			measurements = []

			def cb(sweep_config, sweep):
				assert len(sweep.data) == 1
				measurements.append(sweep.data[0])

				if len(measurements) == 1*step:
					gen.rf_on(f_hz, p_dbm)
					return True
				elif len(measurements) == 2*step:
					gen.rf_off()
					return True
				elif len(measurements) == 3*step:
					return False
				else:
					return True

			name = "settle_time_%ddbm_%dhz" % (p_dbm, f_hz)
			if dut.replay:
				measurements = dut._measure_ch_replay(name)
			else:
				dut.spectrumsensor.run(sweep_config, cb)
				dut._measure_ch_save(name, measurements)

			on_settled = measurements[2*step-step/4:2*step]
			t = get_settle_time(measurements[1*step:], on_settled)

			log("      settled up in %d samples" % (t,))
			if t >= dut.extra:
				log("        WARNING: settle time too long for other tests!")

			off_settled = measurements[-step/4:]
			t = get_settle_time(measurements[2*step:], off_settled)

			log("      settled down in %d samples" % (t,))
			if t >= dut.extra:
				log("        WARNING: settle time too long for other tests!")

	log("End settle time test")

def find_zero(i_start, i_step, x, y):
	i = i_start
	try:
		while y[i] > 0:
			i += i_step
	except IndexError:
		return None

	return numpy.interp(0, y[i-1:i+1], x[i-1:i+1])

def get_channel_start_stop(fc_hz, f_hz_list, pout_dbm_list, config):

	pout_dbm_max = max(pout_dbm_list)
	i_max = pout_dbm_list.index(pout_dbm_max)

	pout_diff_list = numpy.array(pout_dbm_list) - pout_dbm_max + 3.0

	f_hz_start = find_zero(i_max, -1, f_hz_list, pout_diff_list)
	f_hz_stop = find_zero(i_max, +1, f_hz_list, pout_diff_list)

	if f_hz_start is None or f_hz_stop is None:
		log("      Can't calculate channel filter pass band!")
	else:
		bw = f_hz_stop - f_hz_start
		fc_hz_real = (f_hz_stop + f_hz_start) / 2.0

		log("    Poutmax = %f dBm" % (pout_dbm_max,))
		log("    fmin = %f Hz" % (f_hz_start,))
		log("    fmax = %f Hz" % (f_hz_stop,))
		log("    BW(-3 dB) = %f Hz (should be %d Hz, %.1f %%)" % (
				bw, config.bw, 100.0 * (config.bw - bw) / bw))

		efc = fc_hz - fc_hz_real
		log("    Efc = %f Hz (%.1f %% channel)" % (
				efc, 100.0 * efc / config.bw))

def get_noise_figure(f_hz_list, pout_dbm_list, p_dbm, nf_mean):
	pout_list = (10 ** ((numpy.array(pout_dbm_list)-p_dbm) / 10)) * 1e-3
	i = 10*math.log10(scipy.integrate.trapz(pout_list, f_hz_list)/1e-3)
	log("    NF = %f" % ((nf_mean - i) + 174))

def test_ch_filter(dut, gen):
	"""Test channel filter, local oscillator accuracy and noise figure
	"""

	log("Start channel filter test")

	N = 100

	nruns = 3
	ch_num = dut.config.num
	ch_list = [ int(ch_num*(i+0.5)/nruns) for i in xrange(nruns) ]

	p_dbm = -30

	log("  Pin = %d dBm" % (p_dbm,))

	for ch in ch_list:
		fc_hz = dut.config.ch_to_hz(ch)

		log("  fc = %d Hz" % (fc_hz,))

		gen.rf_off()

		nf = dut.measure_ch(ch, N, "channel_filter_%dhz_off" % (fc_hz,))
		nf_mean = numpy.mean(nf)

		log("    N = %f dBm, u = %f" % (nf_mean, numpy.std(nf)))

		npoints = 40

		f_hz_list = [	(fc_hz - 1.5*dut.config.bw) + 3.0 * dut.config.bw / (npoints - 1) * n
				for n in xrange(npoints) ]

		pout_dbm_list = []

		for f_hz in f_hz_list:
			log("    f = %d Hz" % (f_hz))
			gen.rf_on(f_hz, p_dbm)
			s = dut.measure_ch(ch, N, "channel_filter_%dhz_%dhz" % (fc_hz, f_hz))
			s_mean = numpy.mean(s)
			log("      Pout = %f dBm, u = %f" % (s_mean, numpy.std(s)))
			pout_dbm_list.append(s_mean)

		gen.rf_off()

		path = ("log/%s_channel_filter_%dhz.log" % (dut.name, fc_hz))
		f = open(path, "w")
		f.write("# f [Hz]\tPout [dBm]\n")
		for f_hz, pout_dbm in zip(f_hz_list, pout_dbm_list):
			f.write("%f\t%f\n" % (f_hz, pout_dbm))
		f.close()

		get_channel_start_stop(fc_hz, f_hz_list, pout_dbm_list, dut.config)
		get_noise_figure(f_hz_list, pout_dbm_list, p_dbm, nf_mean)

	log("End channel filter test")

def test_ident(dut, gen):
	"""Identify device under test and testing harness
	"""

	log("Start identification")
	log("  Device under test: %s" % (dut.name,))
	if dut.replay:
		log("  *** REPLAY ***")

	resp = dut.spectrumsensor.get_status()
	for line in resp:
		log("    %s" % (line.strip(),))

	log("  Signal generator: %s" % (gen.get_name(),))
	log("End identification")

def run_tests(options):

	dut = DeviceUnderTest(options.vesna_device, options.name,
			replay=options.replay, log_path=options.log_path)
	gen = SignalGenerator(options.usbtmc_device)

	run_all = not any(getattr(options, name) for name, testfunc in iter_tests())

	log("Session started at %s" % (datetime.datetime.now()))

	for name, testfunc in sorted(iter_tests(), key=lambda x:"ident" not in x[0]):
		if run_all or getattr(options, name):
			testfunc(dut, gen)

	log("Session ended at %s" % (datetime.datetime.now()))

def iter_tests():
	for name, testfunc in globals().iteritems():
		if name.startswith("test_") and callable(testfunc):
			yield name, testfunc

def main():
	parser = optparse.OptionParser()
	parser.add_option("-d", "--vesna-device", dest="vesna_device", metavar="DEVICE",
			help="Use VESNA spectrum sensor attached to DEVICE.", default="/dev/ttyUSB0")
	parser.add_option("-g", "--usbtmc-device", dest="usbtmc_device", metavar="DEVICE",
			help="Use signal generator attached to DEVICE.", default="/dev/usbtmc3")
	parser.add_option("-i", "--id", dest="name", metavar="ID",
			help="Use ID as identification for device under test.")
	parser.add_option("-o", "--log", dest="log_path", metavar="PATH", default="log",
			help="Write measurement logs under PATH.")
	parser.add_option("-n", "--replay", dest="replay", action="store_true",
			help="Replay measurement from logs.")

	group = optparse.OptionGroup(parser, "Tests", description="Choose only specific tests to run "
			"(default is to run all tests)")

	for name, testfunc in iter_tests():
		opt = "--" + name.replace("_", "-")
		group.add_option(opt, dest=name, action="store_true", help=testfunc.__doc__)

	parser.add_option_group(group)

	(options, args) = parser.parse_args()

	if not options.name:
		print "Please specify ID for device under test using the \"-i\" option"
		return

	try:
		os.mkdir(options.log_path)
	except OSError:
		pass

	global log_f
	log_f = open("log/%s.log" % (options.name,), "w")

	run_tests(options)

main()

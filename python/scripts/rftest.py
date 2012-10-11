import os
import numpy
import sys
import time
from vesna.spectrumsensor import SpectrumSensor, SweepConfig

class PyUsbTmcError(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class usbtmc:
    """Simple implementation of a USBTMC device interface using the
       linux kernel usbtmc character device driver"""
    def __init__(self, device):
        self.device = device
        try:
            # Get a handle to the IO device
            self.FILE = os.open(device, os.O_RDWR)
        except OSError as e:
            raise PyUsbTmcError("Error opening device: " + str(e))
            # print >> sys.stderr, "Error opening device: ", e
            # raise e
            # TODO: This should throw a more descriptive exception to caller
    
    def write(self, command):
        """Write command directly to the device"""
        try:
            os.write(self.FILE, command);
        except OSError as e:
            print >> sys.stderr, "Write Error: ", e

    def read(self, length=4000):
        """Read an arbitrary amount of data directly from the device"""
        try:
            return os.read(self.FILE, length)
        except OSError as e:
            if e.args[0] == 110:
                print >> sys.stderr, "Read Error: Read timeout"
            else:
                print >> sys.stderr, "Read Error: ", e
            return ""

    def query(self, command, length=300):
        """Write command then read the response and return"""
        self.write(command)
        return self.read(length)

    def getName(self):
        return self.query("*IDN?")

    def sendReset(self):
        self.write("*RST")

    def close(self):
        """Close interface to instrument and release file descriptor"""
        os.close(self.FILE)

class SignalGenerator(usbtmc):
	def rf_on(self, freq_hz, power_dbm):

		power_dbm = max(-145, power_dbm)

		self.write("freq %d Hz\n" % (freq_hz,))
		self.write("pow %d dBm\n" % (power_dbm,))
		self.write("outp on\n")

	def rf_off(self):
		self.write("outp off\n")

class DeviceUnderTest:
	def __init__(self, device, name, replay=False):
		self.name = name
		self.replay = replay
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
		path = ("log/%s_%s.log" % (self.name, name)).replace("-", "m")
		f = open(path, "w")
		f.write("# P [dBm]\n")
		f.write("\n".join(map(str, measurements)))
		f.close()

	def _measure_ch_replay(self, name):
		path = ("log/%s_%s.log" % (self.name, name)).replace("-", "m")
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

def max_error(reference, measured):
	emax = -1
	for vr, vm in zip(reference, measured):
		#vm = numpy.mean(vm)

		emax = max(emax, abs(vr - vm))

	return emax

def test_power_ramp(dut, gen):

	print "Start power ramp test"

	N = 100

	nruns = 3
	ch_num = dut.config.num
	ch_list = [ int(ch_num*(i+0.5)/nruns) for i in xrange(nruns) ]

	for ch in ch_list:
		f_hz = dut.config.ch_to_hz(ch)

		print "  f = %d Hz" % (f_hz)
		
		gen.rf_off()

		nf = dut.measure_ch(ch, N, "power_ramp_%dhz_off" % (f_hz,))
		nf_mean = numpy.mean(nf)

		print "    N = %f dBm, u = %f" % (nf_mean, numpy.std(nf))

		p_dbm_start = int(round(nf_mean/10)*10-30)
		p_dbm_step = 5

		p_dbm_list = range(p_dbm_start, p_dbm_step, p_dbm_step)
		pout_dbm_list = []
		for p_dbm in p_dbm_list:
			print "    Pin = %d dBm" % (p_dbm)
			gen.rf_on(f_hz, p_dbm)
			s = dut.measure_ch(ch, N, "power_ramp_%dhz_%ddbm" % (f_hz, p_dbm))
			s_mean = numpy.mean(s)
			print "       Pout = %f dBm, u = %f" % (s_mean, numpy.std(s))
			pout_dbm_list.append(s_mean)

		gen.rf_off()


		f = open("log/%s_power_ramp_%dhz.log" % (dut.name, f_hz,), "w")
		f.write("# Pin [dBm]\tPout [dBm]\n")
		for p_dbm, pout_dbm in zip(p_dbm_list, pout_dbm_list):
			f.write("%f\t%f\n" % (p_dbm, pout_dbm))
		f.close()

		r, m = chop(p_dbm_list, pout_dbm_list, nf_mean+20, 0)
		print "    Range %.1f - %.1f dBm" % (r[0], r[-1])
		print "      max absolute error %.1f dBm" % (max_error(r, m))

		r, m = chop(p_dbm_list, pout_dbm_list, nf_mean+20, -40)
		print "    Range %.1f - %.1f dBm" % (r[0], r[-1])
		print "      max absolute error %.1f dBm" % (max_error(r, m))

		A = numpy.array([numpy.ones(len(r))])
		n = numpy.linalg.lstsq(A.T, numpy.array(m) - numpy.array(r))[0][0]
		print "      offset = %f dBm" % (n,)

		A = numpy.array([r, numpy.ones(len(r))])
		k, n = numpy.linalg.lstsq(A.T, m)[0]
		print "      linear regression: k = %f, n = %f dBm" % (k, n)

	print "End power ramp test"

def test_freq_sweep(dut, gen):

	print "Start frequency sweep test"

	N = 100

	p_dbm_list = [ -90, -75, -60, -45 ]
	for p_dbm in p_dbm_list:

		print "  Pin = %d dBm" % (p_dbm,)

		nruns = 50
		ch_num = dut.config.num
		ch_list = [ int(ch_num*i/(nruns-1)) for i in xrange(nruns) ]

		f_hz_list = []
		pout_dbm_list = []
		for ch in ch_list:
			f_hz = dut.config.ch_to_hz(ch)
			f_hz_list.append(f_hz)

			print "    f = %d Hz" % (f_hz)
			gen.rf_on(f_hz, p_dbm)
			s = dut.measure_ch(ch, N, "freq_sweep_%ddbm_%dhz" % (p_dbm, f_hz))
			s_mean = numpy.mean(s)
			print "       Pout = %f dBm, u = %f" % (s_mean, numpy.std(s))
			pout_dbm_list.append(s_mean)

		gen.rf_off()

		f = open("log/%s_freq_sweep_%ddbm.log" % (dut.name, p_dbm), "w")
		f.write("# f [Hz]\tPout [dBm]\n")
		for f_hz, pout_dbm in zip(f_hz_list, pout_dbm_list):
			f.write("%f\t%f\n" % (f_hz, pout_dbm))
		f.close()

		print "    Range %.1f - %.1f Hz" % (f_hz_list[0], f_hz_list[-1])
		print "      max absolute error %.1f dBm" % (max_error([p_dbm]*len(f_hz_list), pout_dbm_list))

	print "End power ramp test"

def get_settle_time(measurements, settled):
	mmax = max(settled)
	mmin = min(settled)

	for n, v in enumerate(measurements):
		if v <= mmax and v >= mmin:
			return n

def test_settle_time(dut, gen):
	
	print "Start settle time test"

	N = 1000

	nruns = 3
	ch_num = dut.config.num
	ch_list = [ int(ch_num*(i+0.5)/nruns) for i in xrange(nruns) ]

	p_dbm_list = [ -90, -50, -10 ]

	for p_dbm in p_dbm_list:
		print "  Pin = %d dBm" % (p_dbm,)

		for ch in ch_list:
			f_hz = dut.config.ch_to_hz(ch)

			print "    f = %d Hz" % (f_hz,)

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

			print "      settled up in %d samples" % (t,)
			if t >= dut.extra:
				print "        WARNING: settle time too long for other tests!"

			off_settled = measurements[-step/4:]
			t = get_settle_time(measurements[2*step:], off_settled)

			print "      settled down in %d samples" % (t,)
			if t >= dut.extra:
				print "        WARNING: settle time too long for other tests!"

	print "End settle time test"

def test_identification(dut, gen):
	print "Start identification"
	print "  Device under test: %s" % (dut.name,)
	if dut.replay:
		print "  *** REPLAY ***"

	resp = dut.spectrumsensor.get_status()
	for line in resp:
		print "    %s" % (line.strip(),)

	print "  Signal generator: %s" % (gen.getName(),)
	print "End identification"

def test_all():

	dut = DeviceUnderTest("/dev/ttyUSB0", "sne_ismtv_uhf_sertest", replay=False)
	gen = SignalGenerator("/dev/usbtmc3")

	test_identification(dut, gen)
	test_settle_time(dut, gen)
	test_power_ramp(dut, gen)
	test_freq_sweep(dut, gen)

def main():
	test_all()

main()

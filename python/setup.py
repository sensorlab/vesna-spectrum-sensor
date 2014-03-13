#!/usr/bin/python

from distutils.core import Command, setup
import unittest

UNITTESTS = [
		"tests.tests",
	]

class TestCommand(Command):
	user_options = [ ]

	def initialize_options(self):
		pass

	def finalize_options(self):
		pass

	def run(self):
		suite = unittest.TestSuite()

		suite.addTests(
			unittest.defaultTestLoader.loadTestsFromNames(
								UNITTESTS ) )

		result = unittest.TextTestRunner(verbosity=2).run(suite)

setup(name='vesna-spectrumsensor',
      version='0.1',
      description='Tools for talking the VESNA almost-like-HTTP protocol',
      license='GPL',
      long_description=open("README").read(),
      author='Tomaz Solc',
      author_email='tomaz.solc@tablix.org',

      packages = [ 'vesna', 'vesna.rftest' ],
      provides = [ 'vesna', 'vesna.rftest' ],

      scripts = [ 'scripts/vesna_rftest', 'scripts/vesna_rftest_plot'],

      cmdclass = { 'test': TestCommand }

)

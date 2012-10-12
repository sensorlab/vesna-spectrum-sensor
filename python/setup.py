#!/usr/bin/python

from distutils.core import setup

setup(name='vesna-spectrumsensor',
      version='0.1',
      description='Tools for talking the VESNA almost-like-HTTP protocol',
      license='GPL',
      long_description=open("README").read(),
      author='Tomaz Solc',
      author_email='tomaz.solc@tablix.org',

      packages = [ 'vesna' ],
      provides = [ 'vesna' ],
)

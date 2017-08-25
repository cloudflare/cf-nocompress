#!/usr/bin/env python

import sys

import urllib2

from ngxtest import unit
from difflib import unified_diff
from json import loads
from urllib2 import HTTPError
from breach import Breacher

import simple_env
import unittest
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

class TestNoCompress(unit.TestCase):
    _env = None
    _manage_env = False

    @classmethod
    def setUpClass(cls):
        print "Setting up env"
        cls._env = simple_env.SimpleEnv(log_level='warn')
        pid = cls._env.getpid()
        if pid:
            print >> sys.stderr, "Using running env with pid {}".format(pid)
        else:
            cls._manage_env = True
            cls._env.runAsync()

    @classmethod
    def tearDownClass(cls):
        if cls._manage_env:
            cls._env.stopAsync()


    def get_url(self, name, gzip):
        req = urllib2.Request(self.resolveURL(name, 'https'))
        if gzip:
            req.add_header('Accept-encoding', 'gzip')
        f = urllib2.urlopen(req, context=ctx)
        if gzip:
            self.assertEqual(f.info().get('Content-Encoding'), 'gzip')
        return f.read()


    def _test_compressed(self, name, correct_name, gt):
        data_ef = ''
        try:
            data_ef = self.get_url(name, True)
        except HTTPError as e:
            pass
        with open('t/' + correct_name) as f:
            data_correct = f.read()
        if gt == True and (len(data_ef) >= len(data_correct)):
            self.fail('Compression Error, expected ' + str(len(data_ef)) + ' (' + data_ef + ') to be less than ' + str(len(data_correct)))
        elif gt == False and (len(data_ef) < len(data_correct)):
            self.fail('Compression Error, expected ' + str(len(data_ef)) + ' (' + data_ef + ') to be greater than ' + str(len(data_correct)))

    def _test_helper(self, name, correct_name):
        data_ef = ''
        try:
            data_ef = self.get_url(name, False)
        except HTTPError as e:
            pass
        with open('t/' + correct_name) as f:
            data_correct = f.read()
        if data_ef != data_correct:
            self.fail('Content mismatch\n' + 'origin ' + data_ef + ' end expected ' + data_correct)

    def testHTTPS(self):
        ''' Test expected output with everything disabled and no GZIP support '''
        self._test_helper('/', 'expected/secret')


    def testHTTPSGzip(self):
        ''' Test expected output with everything disabled and GZIP on '''
        self._test_helper('/gzip', 'expected/secret')

    def testDefaultOff(self):
        ''' Test that the filter is a noop if not enabled by LUA '''
        self._test_compressed('/no_compress_but_not_enabled', 'expected/a', True)

    def testNotCompressed(self):
        ''' Test that the filter compresses data if enabled '''
        self._test_compressed('/no_compress_and_is_enabled', 'expected/a', False)

    def testRatioNotRuined(self):
        ''' Test that a small nocompress item does not completely ruin compression ratio '''
        self._test_compressed('/ratio_hit', 'expected/hello', True)

    def testRatioNotRuinedBigger(self):
        ''' Test that a small nocompress item does not completely ruin compression ratio on a large example '''
        self._test_compressed('/ratio_hit2', 'expected/hello_world_repeat', True)

    def testRatioNotRuinedStart(self):
        ''' Test that a small nocompress item does not completely ruin compression ratio when it occurs at the start of the response '''
        self._test_compressed('/ratio_hit_start', 'expected/hello_world_start', True)

    def testBreachDeployment(self):
        ''' Test that we not vulnerable to breach with the deployment key '''
        a = Breacher('1', '2')
        guessed = a.go(self.resolveURL('/canary2?KEY=KEY:', 'https'), 50, 5)
        self.assertEqual(guessed, '')

    def testBreach(self):
        ''' Test that we are vulnerable to BREACH '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/canary?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, 'AZAZA')

    def testBreachBadRule(self):
        ''' Test that we are vulnerable to BREACH when the rule is unrelated '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/bad_rule_canary?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, 'AZAZA')

    def testBreachNoCompress(self):
        ''' Test that we are not vulnerable to BREACH if we attack a protected endpoint '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/safe_canary?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, '')

    def testBreach2(self):
        ''' Test that we are vulnerable to BREACH on a more complex example with a more precise rule '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/canary2?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, 'AZAZA')


    def testBreach2Repeated(self):
        ''' Test that we are vulnerable to BREACH on a more complex example with a more precise rule and the canary repeated '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/canary2_repeated?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, 'AZAZA')

    def testBreachNoCompress2(self):
        ''' Test that we are not vulnerable to BREACH on a more complex example with a more precise rule if we enable nocompress '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/safe_canary2?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, '')

    def testBreachNoCompress2Repeated(self):
        ''' Test that we are not vulnerable to BREACH on a more complex example with a more precise rule if we enable nocompress even when the secret and canary are repeated multiple times '''
        a = Breacher('A', 'Z')
        guessed = a.go(self.resolveURL('/safe_canary2_repeated?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, '')


    def testBreach3(self):
        ''' Test that we are vulnerable to BREACH on a 5 character secret in 50 attempts '''
        a = Breacher('1', '2')
        guessed = a.go(self.resolveURL('/canary3?test=CanaryCrazyFun:', 'https'), 50, 5)
        self.assertEqual(guessed, '11221')

    def testBreachNoCompress3(self):
        ''' Test that we are not vulnerable to BREACH on a 5 character secret in 50 attempts if we enable the feature '''
        a = Breacher('1', '2')
        guessed = a.go(self.resolveURL('/safe_canary3?test=CanaryCrazyFun:', 'https'), 200, 5)
        self.assertEqual(guessed, '')

if __name__ == '__main__':
    TestHTTPS.main(simple_env.SimpleEnv)

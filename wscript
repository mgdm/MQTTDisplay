#!/usr/bin/env python

VERSION='0'
APPNAME='mqttdisplay'

def options(opt):
	opt.tool_options('compiler_cc')

def configure(conf):
	conf.check_tool('compiler_cc')


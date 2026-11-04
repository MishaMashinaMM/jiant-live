# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.18)
project(jiantusb C)
add_library(jiantusb SHARED usbiso.c)

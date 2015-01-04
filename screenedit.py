#!/usr/bin/python
# Python 2.6 or later required

import argparse
import os
import sys
import tempfile

default_bfn = 'forth.blks'
default_editor = os.environ.get('VISUAL')
if default_editor is None:
    default_editor = os.environ.get('EDITOR')
if default_editor is None:
    default_editor = 'emacs'

chars_per_line = 64
lines_per_screen = 16
chars_per_screen = chars_per_line * lines_per_screen


def pad_string_with_spaces (str, desired_len):
    while len (str) < desired_len:
        str += ' '
    return str


def pad_short_screen (buf):
    if len (buf) < chars_per_screen:
        print "block too short, padding"
        buf = pad_string_with_spaces (buf, chars_per_screen)
    return buf
        

def replace_nonprinting_chars_with_space (buf):
    chars = list (buf)
    for i in range (len (chars)):
        c = ord (chars [i])
        if c < 0x20 or c > 0x7e:
            print "char at %d is %x, replacing with space" % (i, c)
            chars [i] = ' '
    return ''.join (chars)


def read_screen (bf, screen_num):
    bf.seek (screen_num * chars_per_screen, os.SEEK_SET)
    buf = bf.read (chars_per_screen)

    # fix possible problems with screen
    buf = pad_short_screen (buf)
    buf = replace_nonprinting_chars_with_space (buf)
    return buf


def write_screen (bf, screen_num, buf):
    bf.seek (screen_num * chars_per_screen, os.SEEK_SET)
    bf.write (buf)


def write_screen_to_text_file (buf):
    #tf = tempfile.NamedTemporaryFile (mode = 'w', delete = False)
    #tfn = tf.name
    (tfd, tfn) = tempfile.mkstemp (text = True)
    tf = os.fdopen (tfd, "w")
    for l in range (lines_per_screen):
        p = l * chars_per_line
        tf.write ("%s\n" % buf [p:p+chars_per_line].rstrip ())
    tf.close ()
    return tfn


def read_screen_from_text_file (tfn):
    tf = open (tfn, "r")
    line_num = 0
    buf = ''
    print "reading"
    for line in tf:
        if line_num >= lines_per_screen:
            break
        line = line.rstrip ()
        line = pad_string_with_spaces (line, chars_per_line)
        buf += line
        line_num += 1
    tf.close ()
    return pad_string_with_spaces (buf, chars_per_screen)


def edit_text_file (fn, editor):
    os.system (editor + ' ' + fn)


parser = argparse.ArgumentParser (description = 'edit a FORTH screen')
                    
parser.add_argument ('screen_num', metavar = 'screen',
                     type = int,
                     help = 'screen number')

parser.add_argument ('--editor', '-e',
                     default = default_editor,
                     help = 'editor program (default: %(default)s)')

parser.add_argument ('--file', '-f',
                     type = argparse.FileType('r+b'),
                     default = default_bfn,
                     help = 'FORTH block file (default: %(default)s)')

args = parser.parse_args()
print args

with args.file as bf:
    buf = read_screen (bf, args.screen_num)

    tfn = write_screen_to_text_file (buf)
    edit_text_file (tfn, args.editor)
    buf2 = read_screen_from_text_file (tfn)
    os.remove (tfn)

    if buf == buf2:
        print "screen unchanged"
    else:
        write_screen (bf, args.screen_num, buf2)

#!/usr/bin/python

# Generate a static fbsplash theme gallery.  Run as:
#  ./gengallery.py > index.html

import os
import xml.etree.ElementTree as et

size = '1024x768'
tsize = '300x225'

print '<table>'

for theme in sorted(os.listdir('unpacked')):
	if not os.path.isdir('unpacked/' + theme):
		continue

	# Only themes with valid metadata are considered.
	try:
		f = open('unpacked/' + theme + '/metadata.xml', 'r')
		meta = et.parse(f)
	except Exception:
		continue

	url = None
	name = meta.find('name').text
	ver = meta.find('version').text
	author = meta.find('author/name').text
	email = meta.find('author/email').text
	desc = meta.find('description').text
	lic = meta.find('license').text

	t = meta.find('url')
	if t is not None:
		url = t.text

	shot  = '../themes/shots/%s-%s.png' % (size, theme)
	shott = '../themes/shots/thumbs/%s-%s.jpg' % (tsize, theme)

	print '<tr><td><a href="%s"><img src="%s" alt="%s" /></a>' % (shot, shott, name)

	fbcd  = '../themes/shots/%s-%s-fbcondecor.png' % (size, theme)
	fbcdt = '../themes/shots/thumbs/%s-%s-fbcondecor.jpg' % (tsize, theme)

	if os.path.exists(fbcd):
		print '<a href="%s"><img src="%s" alt="%s fbcondecor" /></a>' % (fbcd, fbcdt, name)

	print '<br /><span class="theme">'
	if url is not None:
		print '<a href="%s">%s v. %s</a>,' % (url, name, ver)
	else:
		print '%s v. %s,' % (name, ver)

	print '<b><a href="../themes/repo/%s.tar.bz2">download</a></b>' % theme

	print '</span><br /><span class="desc">%s</span><br /><br />' % desc

	print '</td></tr>'

	f.close()

print '</table>'

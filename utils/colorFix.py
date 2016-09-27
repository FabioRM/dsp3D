import sys, time, os

num = 20
name = 'star'

f = open('a.txt', 'w+')


for i in range(num):
	s = '\t' + name + '.facesColor['+str(i)+'][0] = 0xFF;\r\n'
	f.write(s)
	s = '\t' + name + '.facesColor['+str(i)+'][1] = 0xFF;\r\n'
	f.write(s)
	s = '\t' + name + '.facesColor['+str(i)+'][2] = 0xFF;\r\n'
	f.write(s)

f.close()
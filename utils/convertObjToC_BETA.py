'''
Copyright (c) 2016 - Fabio Angeletti
e-mail: fabio.angeletti89@gmail.com
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of dsp3D nor the names of its contributors may be used
  to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This work was inspired by the excellent tutorial series by David Rousset:
"learning how to write a 3D soft engine from scratch in C#, TypeScript
or JavaScript" - available on David's website https://www.davrous.com
'''

'''
use this file to generate dsp3D engine models from wavefront .obj files

'''

import glob, os

for file in glob.glob('*.obj'):
	name = file.replace('.obj', '')

	f_input = open(file, 'r')
	fi_lines = f_input.readlines()
	f_input.close()

	f_output = open(name + '.c', 'w+')

	vNum = 0
	vnNum = 0
	fNum = 0

	vDict = {}
	vnDict = {}
	fDict = {}

	f_output.write('#include \"' + name + '.h\"\n\n')
	f_output.write('genericMesh ' + name + ';\n\n');
	f_output.write('void init' + name.title() + '(void)\n{\n')

	for line in fi_lines:
		line_data = line.strip().split(' ')
		if(line_data[0] == 'v'):
			s = '\t' + name + '.vertices['+ str(vNum) + '][0] = ' + line_data[1] + ';\n'
			f_output.write(s);
			s = '\t' + name + '.vertices['+ str(vNum) + '][1] = ' + line_data[2] + ';\n'
			f_output.write(s);
			s = '\t' + name + '.vertices['+ str(vNum) + '][2] = ' + line_data[3] + ';\n'
			f_output.write(s);
			vNum = vNum + 1

		if(line_data[0] == 'vn'):
			s = '\t' + name + '.verticesNormal['+ str(vnNum) + '][0] = ' + line_data[1] + ';\n'
			f_output.write(s);
			s = '\t' + name + '.verticesNormal['+ str(vnNum) + '][1] = ' + line_data[2] + ';\n'
			f_output.write(s);
			s = '\t' + name + '.verticesNormal['+ str(vnNum) + '][2] = ' + line_data[3] + ';\n'
			f_output.write(s);
			vnNum = vnNum + 1
		
		if(line_data[0] == 'f'):
			s = '\t' + name + '.faces['+ str(fNum) + '][0] = ' + str(int(line_data[1].split('/')[0]) - 1) + ';\n'
			f_output.write(s);
			s = '\t' + name + '.faces['+ str(fNum) + '][1] = ' + str(int(line_data[2].split('/')[0]) - 1) + ';\n'
			f_output.write(s);
			s = '\t' + name + '.faces['+ str(fNum) + '][2] = ' + str(int(line_data[3].split('/')[0]) - 1) + ';\n'
			f_output.write(s);
			fNum = fNum + 1

	f_output.write('\n\t' + name + '.numVert = ' + str(vNum) + ';\n')
	f_output.write('\t' + name + '.numFaces = ' + str(fNum) + ';\n')

	f_output.write('}\n')
	f_output.close()

	f_output = open(name + '.h', 'w+')
	f_output.write('#ifndef __' + name.upper() + '_H__\n')
	f_output.write('#define __' + name.upper() + '_H__\n\n')

	f_output.write('#include "dsp3d.h"\n')
	f_output.write('#include "genericMesh.h"\n\n')

	f_output.write('void init' + name.title() + '(void);\n\n')
	f_output.write('#endif\n')

	f_output.close()

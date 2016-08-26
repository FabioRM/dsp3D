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
use this file to generate dsp3D engine models from babylon exports
within blender

see the tutorial on David's website https://www.davrous.com for more info
'''

import glob, os

for file in glob.glob('*.babylon'):
	name = file.replace('.babylon', '')

	f_input = open(file, 'r')
	fi_data = f_input.read()
	vStartIndex = fi_data.find('vertices')
	vStartIndex += fi_data[vStartIndex+1:].find('[') + 2
	vEndIndex = vStartIndex+fi_data[vStartIndex:].find(']')
	iStartIndex = fi_data.find('indices')
	iStartIndex += fi_data[iStartIndex+1:].find('[') + 2
	iEndIndex = iStartIndex+fi_data[iStartIndex:].find(']')
	f_input.close()

	vertices = fi_data[vStartIndex:vEndIndex].split(',')
	indices = fi_data[iStartIndex:iEndIndex].split(',')

	f_input.close()

	f_output = open(name + '.c', 'w+')

	verticesLength = len(vertices)
	indicesLength = len(indices)

	f_output.write('#include \"' + name + '.h\"\n\n')

	f_output.write('genericMesh ' + name + ';\n\n');

	f_output.write('void init' + name.title() + '(void)\n{\n')

	f_output.write('\t' + name + '.numVert = ' + str(verticesLength / 6) + ';\n')
	f_output.write('\t' + name + '.numFaces = ' + str(indicesLength / 3) + ';\n\n')

	vertex = [0, 0, 0]
	face = [0, 0, 0]

	for index in range(0, verticesLength / 6):
		s = '\t' + name + '.vertices['+ str(index) + '][0] = ' + str(vertices[index * 6]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.vertices['+ str(index) + '][1] = ' + str(vertices[index * 6 + 1]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.vertices['+ str(index) + '][2] = ' + str(vertices[index * 6 + 2]) + ';\n'
		f_output.write(s);

	f_output.write('\n')
	
	for index in range(0, verticesLength / 6):	
		s = '\t' + name + '.verticesNormal['+ str(index) + '][0] = ' + str(vertices[index * 6 + 3]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.verticesNormal['+ str(index) + '][1] = ' + str(vertices[index * 6 + 4]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.verticesNormal['+ str(index) + '][2] = ' + str(vertices[index * 6 + 5]) + ';\n'
		f_output.write(s);

	f_output.write('\n')

	for index in range(0, indicesLength / 3):
		s = '\t' + name + '.faces['+ str(index) + '][0] = ' + str(indices[index * 3]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.faces['+ str(index) + '][1] = ' + str(indices[index * 3 + 1]) + ';\n'
		f_output.write(s);
		s = '\t' + name + '.faces['+ str(index) + '][2] = ' + str(indices[index * 3 + 2]) + ';\n'
		f_output.write(s);

	f_output.write('\n}\n')
	f_output.close()

	f_output = open(name + '.h', 'w+')
	f_output.write('#ifndef __' + name.upper() + '_H__\n')
	f_output.write('#define __' + name.upper() + '_H__\n\n')

	f_output.write('#include "dsp3d.h"\n')
	f_output.write('#include "genericMesh.h"\n\n')

	f_output.write('void init' + name.title() + '(void);\n\n')
	f_output.write('#endif\n')

	f_output.close()

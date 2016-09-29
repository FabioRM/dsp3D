"""
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
"""

bl_info = {
	"name": "dsp3D model",
	"author": "Fabio Angeletti",
	"version": (1, 0),
	"blender": (2, 67, 0),
	"location": "File > Export > dsp3D model",
	"description": "Export dsp3D model",
	"warning": "",
	"wiki_url": "",
	"tracker_url": "",
	"category": "Import-Export"}
	
import math
import os
import bpy
import string
import bpy_extras.io_utils
from bpy.props import *
import mathutils, math
import struct
import shutil
from os import remove
from bpy_extras.io_utils import (ExportHelper, axis_conversion)
from bpy.props import (BoolProperty, FloatProperty, StringProperty, EnumProperty, FloatVectorProperty)

class SubMesh:
	materialIndex = 0
	verticesStart = 0
	verticesCount = 0
	indexStart = 0
	indexCount = 0
	
class MultiMaterial:
	name = ""
	materials = []

class Export_CModel(bpy.types.Operator, ExportHelper):  
	"""Export Babylon.js scene (.babylon)""" 
	bl_idname = "scene.c"
	bl_label = "Export dsp3D model"

	filename_ext = ".c"
	filepath = ""
	
	# global_scale = FloatProperty(name="Scale", min=0.01, max=1000.0, default=1.0)

	def execute(self, context):
		   return Export_CModel.save(self, context, **self.as_keywords(ignore=("check_existing", "filter_glob", "global_scale")))
		   
	def mesh_triangulate(mesh):
		import bmesh
		bm = bmesh.new()
		bm.from_mesh(mesh)
		bmesh.ops.triangulate(bm, faces=bm.faces)
		bm.to_mesh(mesh)
		mesh.calc_tessface()
		bm.free()

	def export_mesh(object, scene, file_handler, fileName):
		materialsDict = dict()
		verticesDict = dict()
		facesDict = dict()

		# Get mesh      
		mesh = object.to_mesh(scene, True, "PREVIEW")
		
		# Transform
		matrix_world = object.matrix_world.copy()
		matrix_world.translation = mathutils.Vector((0, 0, 0))
		mesh.transform(matrix_world)            
								
		# Triangulate mesh if required
		Export_CModel.mesh_triangulate(mesh)

		# Materials
		materials = [mat for mat in bpy.data.materials if mat.users >= 1]
		materialDict = dict()
		materialIndex = 0
		for material in materials:
			r = int((material.ambient * material.diffuse_color)[0] * 255)
			g = int((material.ambient * material.diffuse_color)[1] * 255)
			b = int((material.ambient * material.diffuse_color)[2] * 255)
			materialDict[materialIndex] = (r,g,b)
			materialIndex = materialIndex + 1

		materialsCount = max(1, len(object.material_slots))
		verticesCount = len(mesh.vertices)
		facesCount = len(mesh.tessfaces)

		file_handler.write(str(verticesCount) + ', ' + str(facesCount) + ', ')

		materialIndex = 0
		verticesIndex = 0
		facesIndex = 0

		for materialIndex in range(materialsCount):
			for face in mesh.tessfaces:
				if face.material_index != materialIndex:
					continue
				faceMaterial = materialDict[face.material_index]
				faceVertices = []
				for v in range(3): # For each vertex in face
					vArr = []
					vertex_index = face.vertices[v]
					vertex = mesh.vertices[vertex_index]
					position = vertex.co
					normal = vertex.normal 

					vArr.append(position.x)
					vArr.append(position.y)
					vArr.append(position.z)
					vArr.append(normal.x)
					vArr.append(normal.y)
					vArr.append(normal.z)
					verticesDict[vertex_index] = vArr

					faceVertices.append(vertex_index)

				fData = []

				for v in faceVertices:
					fData.append(v)

				for m in faceMaterial:
					fData.append(m)
				
				facesDict[facesIndex] = fData
				facesIndex = facesIndex + 1

		for i in verticesDict:
			for j in range(6):
				file_handler.write('%.4f, ' % verticesDict[i][j])

		for i in facesDict:
			for j in range(6):
				file_handler.write('%.4f, ' % facesDict[i][j])

	def save(operator, context, filepath="",
		use_apply_modifiers=False,
		use_triangulate=True,
		use_compress=False):

		# Open file
		file_handler = open(filepath, 'w')	
		fileName = os.path.basename(filepath).split('.')[0]

		retStr = ""
		retStr = retStr + '#ifndef __' + fileName.upper() + '_C__\n'
		retStr = retStr + '#define __' + fileName.upper() + '_C__\n\n'
		retStr = retStr + '#include "stdint.h"\n'
		retStr = retStr + '#define float32_t float\n\n'
		retStr = retStr + 'float32_t ' + fileName + 'ModelData[] = {'
		
		file_handler.write(retStr)

		if bpy.ops.object.mode_set.poll():
			bpy.ops.object.mode_set(mode='OBJECT')		
			bpy.ops.object.origin_set(type='ORIGIN_CURSOR')  

		# Writing scene
		scene=context.scene
		
		world = scene.world
		if world:
			world_ambient = world.ambient_color
		else:
			world_ambient = Color((0.0, 0.0, 0.0))

		# Mesh
		for object in [object for object in scene.objects]:
			if (object.type == 'MESH'):
				Export_CModel.export_mesh(object, scene, file_handler, fileName)
		
		file_handler.write(' };\n\n#endif\n')

		# Closing file
		file_handler.close()
		
		return {'FINISHED'}

# UI
bpy.types.Mesh.checkCollisions = BoolProperty(
	name="Check collisions", 
	default = False)
	
bpy.types.Camera.checkCollisions = BoolProperty(
	name="Check collisions", 
	default = False)
	
bpy.types.Camera.applyGravity = BoolProperty(
	name="Apply Gravity", 
	default = False)    
	
bpy.types.Camera.ellipsoid = FloatVectorProperty(
	name="Ellipsoid", 
	default = mathutils.Vector((0.2, 0.9, 0.2)))            

class ObjectPanel(bpy.types.Panel):
	bl_label = "dsp3D model"
	bl_space_type = "PROPERTIES"
	bl_region_type = "WINDOW"
	bl_context = "data"
	
	def draw(self, context):
		ob = context.object
		if not ob or not ob.data:
			return
			
		layout = self.layout
		isMesh = isinstance(ob.data, bpy.types.Mesh)
		isCamera = isinstance(ob.data, bpy.types.Camera)
		
		if isMesh:
			layout.prop(ob.data, 'checkCollisions')         
		elif isCamera:
			layout.prop(ob.data, 'checkCollisions')
			layout.prop(ob.data, 'applyGravity')
			layout.prop(ob.data, 'ellipsoid')
			
### REGISTER ###

def menu_func(self, context):
	self.layout.operator(Export_CModel.bl_idname, text="dsp3D model (.c)")

def register():
	bpy.utils.register_module(__name__)
	bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
	bpy.utils.unregister_module(__name__)
	bpy.types.INFO_MT_file_export.remove(menu_func)

	
if __name__ == "__main__":
	register()

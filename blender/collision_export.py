
# This file is adapted from the source here: https://code.google.com/archive/p/blender-scripts/wikis/ExportPhysicsShapes.wiki
# The repository lists the source as under the New BSD license. The original source file did not contain this copyright, so I hope
# adding it to my copy is the right thing to do.


# Copyright 2012 Stuart Denman(stuart.denman@gmail.com)

# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#



bl_info = {
    "name": "Export collision shapes (.yaml)",
    "author": "Stuart Denman",
    "version": (1, 0),
    "blender": (2, 80, 0),
    "location": "File > Export",
	"description": "Export collision shapes under the selected empty named 'physics'.",
	"category": "Import-Export"
}


'''
Blender Installation
Download the script to your hard drive. Right click on collision_export.py and Save As...
In Blender User Preferences > Addons tab, click the "Install..." button and select the collision_export.py script file.
Alternatively, copy the collision_export.py script file to your blender addons folder and enable it in User Preferences > Addons.
See blender documentation for more information.


Usage Notes:
To create a compound physics collision shape for a mesh in blender:

1. Place the 3D cursor at the origin of the mesh object.
2. Add > Empty, name it "physics"
3. Make the new empty a child of your mesh object.
4. Create a physics shape with Add > Mesh > Cube, UV Sphere, Cylinder, Cone or create an arbitrary mesh for a ConvexHull shape.
5. Parent the new shape to the "physics" Empty.
6. The mesh name must start with: Box/Cube, Sphere, Cylinder, Cone, Capsule, or ConvexHull, depending on the shape you want.
7. Position and scale the shape object, but do not modify the internal vertices, unless it is a ConvexHull type.
8. Repeat step 3-6 until your shape is complete. Shapes can only be a 1-level deep hierarchy.
9. IMPORTANT: Select the empty object you named "physics"
10. Click File > Export > Export collision shapes (.yaml)
'''



import bpy



# ExportHelper is a helper class, defines filename and
# invoke() function which calls the file selector.
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty
from bpy.types import Operator
import mathutils, math, struct
from mathutils import *


# Methods for writing point, scale, and quaternion types to a YAML file.
# This particular implementation converts values to a Y-up coordinate system.
def out_point3_y_up( v ):
    return "[%g,%g,%g]" % ( v.x, v.z, -v.y )
def out_scale3_y_up( s ):
    return "[%g,%g,%g]" % ( s.x, s.z, s.y )
def out_quaternion_y_up( q ):
    return "[%g,%g,%g,%g]" % ( q.w, q.x, q.z, -q.y )
    
    
# This implementation maintains blender's Z-up coordinate system.
def out_point3_z_up( v ):
    return "[%g,%g,%g]" % ( v.x, v.y, v.z )
def out_scale3_z_up( s ):
    return "[%g,%g,%g]" % ( s.x, s.y, s.z )
def out_quaternion_z_up( q ):
    return "[%g,%g,%g,%g]" % ( q.w, q.x, q.y, q.z )


def getPhysicsShape(obj, use_y_up):
    shape = ""
    props = { }
    name = obj.name.lower()
    scale = Vector(( abs(obj.scale.x), abs(obj.scale.y), abs(obj.scale.z) ))

    if use_y_up:
        out_point3 = out_point3_y_up
        out_scale3 = out_scale3_y_up
        out_quaternion = out_quaternion_y_up
    else:
        out_point3 = out_point3_z_up
        out_scale3 = out_scale3_z_up
        out_quaternion = out_quaternion_z_up	
    
    # BOX
    if name.startswith('box') or name.startswith('cube'):
        shape = "Box"
        props["half-extents"] = out_scale3( scale )
    # SPHERE
    elif name.startswith('sph'):
        shape = "Sphere"
        props["radius"] = obj.scale.x
    # CONE
    elif name.startswith('cone'):
        shape = "Cone"
        props["radius"] = obj.scale.x
        props["height"] = obj.scale.z * 2.0
    # CYLINDER
    elif name.startswith('cyl'):
        shape = "Cylinder"
        props["half-extents"] = out_scale3( scale )
    # CAPSULE
    elif name.startswith('cap'):
        shape = "Capsule"
        props["radius"] = obj.scale.x
        props["height"] = obj.scale.z
    # CONVEX-HULL
    elif name.startswith('convex'):
        shape = "ConvexHull"
        mesh = obj.to_mesh( bpy.context.scene, True, 'PREVIEW' )
        props["points"] = "\n"
        for v in mesh.vertices:
            props["points"] += "    - " + out_point3( v.co ) + "\n"
        props["points"] = props["points"].rstrip("\n")
        if scale != Vector((1,1,1)):
            props["scale"] = out_scale3( scale )
        # remove mesh
    
    if obj.location != Vector((0,0,0)):
        props["origin"] = out_point3( obj.location )
    if obj.rotation_mode == 'QUATERNION':
        qrot = obj.rotation_quaternion
    else:
        qrot = obj.matrix_local.to_quaternion()
    if qrot != Quaternion((1,0,0,0)):
        props["rotate"] = out_quaternion( qrot )
    
    return (shape, props)


class ExportCollisionShapes(Operator, ExportHelper):

    bl_idname = "object.export_collision"
    bl_label = "Export collision shapes"

    # ExportHelper mixin class uses this
    filename_ext = ".yaml"

    filter_glob: StringProperty(
        default="*.yaml",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )


    use_y_up : BoolProperty(name="Convert To Y-Up",
                            description="Converts the values to a Y-Axis Up coordinate system",
                            default=True)

    append_to_existing : BoolProperty(name="Append To Existing File",
                            description="Appends the physics shapes to an existing file",
                            default=False)

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type in ['EMPTY'] and context.active_object.name == 'physics'


    def execute(self, context):
        obj = context.active_object
        
        f = open(self.filepath, ["w","a"][self.append_to_existing], encoding='utf-8')
        f.write( obj.parent.name + ":\n" )
        f.write( "  physics:\n" )
        
        for c in obj.children:
            if c.type != 'MESH':
                continue
            (shape, props) = getPhysicsShape(c, self.use_y_up)
            f.write("  - shape: " + shape + "\n")
            for (k,v) in props.items():
                f.write("    %s: %s\n" % (k, v))

        f.close()
        self.report( {'INFO'}, "Export succeeded!" )
        return {'FINISHED'}



def menu_func_export(self, context):
    self.layout.operator(ExportCollisionShapes.bl_idname, text="Export collision shapes (.yaml)")


def register():
    bpy.utils.register_class(ExportCollisionShapes)
    #bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_class(ExportCollisionShapes)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()

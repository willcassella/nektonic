# ui.py

import bpy
from bpy.types import Panel
from bpy.props import PointerProperty
from . import types, operators

class SinGEDEntityPanel(Panel):
    bl_label = 'SinGED entity'
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'object'

    @classmethod
    def poll(cls, context):
        # If there is no active connection
        if types.SinGEDProps.sge_session is None:
            return False

        # If the current object does not have an entity id, don't draw the panel
        if context.active_object.sge_entity_id == 0:
            return False

        # Draw the panel
        return True

    def draw(self, context):
        layout = self.layout
        layout.prop(context.scene.singed.sge_types, 'sge_component_types')
        op = layout.operator(operators.SinGEDNewComponent.bl_idname)
        op.entity_id = context.active_object.sge_entity_id
        op.component_type = context.scene.singed.sge_types.sge_component_types
        return

class SinGEDComponentPanelBase(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'object'

    @classmethod
    def sge_unregister(cls):
        bpy.utils.unregister_class(cls)

    @classmethod
    def poll(cls, context):
        component_name = cls.sge_blender_type.sge_type_name
        obj = context.active_object

        # If this object doesn't have an entity id, don't draw the panel
        if obj.sge_entity_id == 0:
            return False

        # If this object doesn't have this type of component attached, don't draw the panel
        if component_name not in types.SinGEDProps.sge_scene.get_components(obj.sge_entity_id):
            return False

        # Draw the panel
        return True

    def draw(self, context):
        self.sge_blender_type.sge_draw(self.layout, bpy.context.scene.singed.sge_types, self.sge_blender_type.__name__)

def create_blender_component(typedb, type_name, blender_type):
     # Add the type to the types class
    setattr(types.SGETypes, blender_type.__name__, PointerProperty(type=blender_type))

    # Create a dictionary for the panel type
    panel_class_dict = {
        'bl_label': type_name,
        'sge_blender_type': blender_type,
    }

    # Create the panel type
    panel_type_name = "{}_panel".format(blender_type.__name__)
    blender_panel_type = type(panel_type_name, (SinGEDComponentPanelBase,), panel_class_dict)

    # Register it with blender
    bpy.utils.register_class(blender_panel_type)

    # Add it to the typedb
    typedb.insert_type(panel_type_name, blender_panel_type)

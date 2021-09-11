// -----------------------------------------------------------------------------
// in-game editor
// - rlyeh, public domain.
//
// when editor is on:
// - [ ] ESC, toggles editor on/off
// - [ ] ESC (long press), sends escape key to game
// - [ ] F5, toggles game pause on/off
// - [ ] F5 (long press), restarts game
// - [ ] TAB, see console/log
// - [ ] LMB, select item, display hierarchy @ left sidebar, status @ right sidebar. right click to unselect
// - [ ] RMB, on selected item, display contextual menu: edit, edit script...
// - [ ] RMB, camera move if no gameobj is selected + WASDEC + wheel
// - [ ] SPC, on selected item, cycle transform: locate, rotate, scale
// - [ ] CTRL+S save, CTRL+L load restore point
// - [ ] IMPR save capture bug/info
//
// roadmap
// - [x] ray/object picking
// - [x] basic gizmos (@todo: fixed screen size, snapping)
// - [ ] introspect properties on selected object
// - [ ] add/rem entities, add/rem components, add/rem/pause/resume systems
// - [ ] widgets for (bool,str,flt,int,vec2,vec3,vec4,range,mesh,texture,audio,any other asset,combo of anything)
// - [ ] osc server for properties and editor behavior
// - [ ] multiple selections/select all
// - [ ] undo/redo
// - [ ] cut/copy/paste (ctrl-c to serialize)
// - [ ] menu: open, save, save as, save all, reload

API void editor();
API bool editor_active();

API int  gizmo(vec3 *pos, vec3 *rot, vec3 *sca);
API bool gizmo_active();


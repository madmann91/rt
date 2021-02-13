#ifndef IO_IMPORT_OBJ_H
#define IO_IMPORT_OBJ_H

struct scene;
struct mesh;
struct obj_model;

/*
 * Imports the given obj model as a mesh, along with all the referenced materials.
 * Since OBJ materials are not physically correct in general, this function tries
 * to emulate them with physically correct ones as much as possible, but this means
 * that some features of OBJ materials like ambient color are not supported.
 * This function can return `NULL` if the file cannot be opened, or if it contains
 * errors. If there are no errors, the function returns a valid mesh which must be
 * freed using `free_mesh()`.
 */
struct mesh* import_obj_model(struct scene* scene, const char* file_name);

#endif

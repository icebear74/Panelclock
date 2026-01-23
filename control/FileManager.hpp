#ifndef FILE_MANAGER_HPP
#define FILE_MANAGER_HPP

#include <Arduino.h>

/**
 * FileManager - Route registration for the lightweight LittleFS file manager UI and API.
 *
 * Call setupFileManagerRoutes() after server has been created.
 *
 * This module uses the global 'server' and LittleFS instances from the application.
 * It registers:
 *  - GET  /fs           -> UI (HTML)
 *  - GET  /fs/list      -> JSON listing of directory
 *  - GET  /fs/download  -> download a file
 *  - DELETE /fs/delete  -> delete a file
 *  - POST /fs/upload    -> upload (multipart/form-data, chunked)
 *  - POST /fs/mkdir     -> create directory
 *
 * The JSON returned by /fs/list contains both numeric size ("size_bytes") and a
 * human-readable string ("size_readable").
 */

void setupFileManagerRoutes();

#endif // FILE_MANAGER_HPP
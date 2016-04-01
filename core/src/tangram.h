#pragma once

#include "debug.h"
#include "data/properties.h"
#include "util/ease.h"
#include <memory>
#include <vector>
#include <string>

/* Tangram API
 *
 * Primary interface for controlling and managing the lifecycle of a Tangram
 * map surface
 */

namespace Tangram {

class DataSource;

// Create resources and initialize the map view using the scene file at the
// given resource path
void initialize(const char* scenePath);

// Initialize graphics resources; OpenGL context must be created prior to calling this
void setupGL();

// Resize the map view to a new width and height (in pixels)
void resize(int newWidth, int newHeight);

// Update the map state with the time interval since the last update
void update(float dt);

// Render a new frame of the map view (if needed)
void render();

// Set the position of the map view in degrees longitude and latitude; if duration
// (in seconds) is provided, position eases to the set value over the duration;
// calling either version of the setter overrides all previous calls
void setPosition(double lon, double lat);
void setPosition(double lon, double lat, float duration, EaseType e = EaseType::quint);

// Set the values of the arguments to the position of the map view in degrees
// longitude and latitude
void getPosition(double& lon, double& lat);

// Set the fractional zoom level of the view; if duration (in seconds) is provided,
// zoom eases to the set value over the duration; calling either version of the setter
// overrides all previous calls
void setZoom(float z);
void setZoom(float z, float duration, EaseType e = EaseType::quint);

// Get the fractional zoom level of the view
float getZoom();

// Set the counter-clockwise rotation of the view in radians; 0 corresponds to
// North pointing up; if duration (in seconds) is provided, rotation eases to the
// the set value over the duration; calling either version of the setter overrides
// all previous calls
void setRotation(float radians);
void setRotation(float radians, float duration, EaseType e = EaseType::quint);

// Get the counter-clockwise rotation of the view in radians; 0 corresponds to
// North pointing up
float getRotation();

// Set the tilt angle of the view in radians; 0 corresponds to straight down;
// if duration (in seconds) is provided, tilt eases to the set value over the
// duration; calling either version of the setter overrides all previous calls
void setTilt(float radians);
void setTilt(float radians, float duration, EaseType e = EaseType::quint);

// Get the tilt angle of the view in radians; 0 corresponds to straight down
float getTilt();

// Transform coordinates in screen space (x right, y down) into their longitude
// and latitude in the map view
void screenToWorldCoordinates(double& x, double& y);

// Set the ratio of hardware pixels to logical pixels (defaults to 1.0)
void setPixelScale(float pixelsPerPoint);

// Set the CameraType based on the cameraType value
void setCameraType(uint8_t cameraType);

// Get the current CameraType of the view
uint8_t getCameraType();

// Add a data source for adding drawable map data, which will be styled
// according to the scene file using the provided data source name;
void addDataSource(std::shared_ptr<DataSource> source);

// Remove a data source from the map; returns true if the source was found
// and removed, otherwise returns false.
bool removeDataSource(DataSource& source);

void clearDataSource(DataSource& source, bool data, bool tiles);

// Respond to a tap at the given screen coordinates (x right, y down)
void handleTapGesture(float posX, float posY);

// Respond to a double tap at the given screen coordinates (x right, y down)
void handleDoubleTapGesture(float posX, float posY);

// Respond to a drag with the given displacement in screen coordinates (x right, y down)
void handlePanGesture(float startX, float startY, float endX, float endY);

// Respond to a fling from the given position with the given velocity in screen coordinates
void handleFlingGesture(float posX, float posY, float velocityX, float velocityY);

// Respond to a pinch at the given position in screen coordinates with the given
// incremental scale
void handlePinchGesture(float posX, float posY, float scale, float velocity);

// Respond to a rotation gesture with the given incremental rotation in radians
void handleRotateGesture(float posX, float posY, float rotation);

// Respond to a two-finger shove with the given distance in screen coordinates
void handleShoveGesture(float distance);

// Set debug features on or off using a boolean (see debug.h)
void setDebugFlag(DebugFlags flag, bool on);

// Get the boolean state of a debug feature (see debug.h)
bool getDebugFlag(DebugFlags flag);

// Toggle the boolean state of a debug feature (see debug.h)
void toggleDebugFlag(DebugFlags flag);

void loadScene(const char* scenePath, bool setPositionFromScene = false);

void runOnMainLoop(std::function<void()> task);

struct TouchItem {
    std::shared_ptr<Properties> properties;
    float position[2];
    float distance;
};

const std::vector<TouchItem>& pickFeaturesAt(float x, float y);

float frameTime();

}


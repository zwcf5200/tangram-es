package com.mapzen.tangram.android;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Window;
import android.widget.Toast;

import com.mapzen.tangram.HttpHandler;
import com.mapzen.tangram.LngLat;
import com.mapzen.tangram.MapController;
import com.mapzen.tangram.MapController.FeaturePickListener;
import com.mapzen.tangram.MapController.FrameCaptureCallback;
import com.mapzen.tangram.MapController.ViewCompleteListener;
import com.mapzen.tangram.MapData;
import com.mapzen.tangram.MapView;
import com.mapzen.tangram.MapView.OnMapReadyCallback;
import com.mapzen.tangram.TouchInput.DoubleTapResponder;
import com.mapzen.tangram.TouchInput.LongPressResponder;
import com.mapzen.tangram.TouchInput.TapResponder;
import com.squareup.okhttp.Callback;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class MainActivity extends Activity implements OnMapReadyCallback, TapResponder,
        DoubleTapResponder, LongPressResponder, FeaturePickListener {

    MapController map;
    MapView view;
    LngLat lastTappedPoint;
    MapData markers;
    boolean showTileInfo = false;

    final static String tileApiKey = "?api_key=vector-tiles-tyHL4AY";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);

        setContentView(R.layout.main);

        view = (MapView)findViewById(R.id.map);
        view.onCreate(savedInstanceState);
        view.getMapAsync(this, "scene.yaml");
    }

    @Override
    public void onResume() {
        super.onResume();
        view.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        view.onPause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        view.onDestroy();
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        view.onLowMemory();
    }

    @Override
    public void onMapReady(MapController mapController) {
        map = mapController;
        map.setZoom(16);
        map.setPosition(new LngLat(-74.00976419448854, 40.70532700869127));
        map.setHttpHandler(getHttpHandler());
        map.setTapResponder(this);
        map.setDoubleTapResponder(this);
        map.setLongPressResponder(this);
        map.setFeaturePickListener(this);

        map.setViewCompleteListener(new ViewCompleteListener() {
                public void onViewComplete() {
                    runOnUiThread(new Runnable() {
                            public void run() {
                                Log.d("Tangram", "View complete");
                            }
                        });
                }});
        markers = map.addDataLayer("touch");
    }

    HttpHandler getHttpHandler() {
        HttpHandler handler = new HttpHandler() {
            @Override
            public boolean onRequest(String url, Callback cb) {
                url += tileApiKey;
                return super.onRequest(url, cb);
            }

            @Override
            public void onCancel(String url) {
                url += tileApiKey;
                super.onCancel(url);
            }
        };

        File cacheDir = getExternalCacheDir();
        if (cacheDir != null && cacheDir.exists()) {
            handler.setCache(new File(cacheDir, "tile_cache"), 30 * 1024 * 1024);
        }

        return handler;
    }

    @Override
    public boolean onSingleTapUp(float x, float y) {
        return false;
    }

    @Override
    public boolean onSingleTapConfirmed(float x, float y) {
        LngLat tappedPoint = map.coordinatesAtScreenPosition(x, y);

        if (lastTappedPoint != null) {
            Map<String, String> props = new HashMap<>();
            props.put("type", "line");
            props.put("color", "#D2655F");

            List<LngLat> line = new ArrayList<>();
            line.add(lastTappedPoint);
            line.add(tappedPoint);
            markers.addPolyline(line, props);

            props = new HashMap<>();
            props.put("type", "point");
            markers.addPoint(tappedPoint, props);
        }

        lastTappedPoint = tappedPoint;

        map.pickFeature(x, y);

        map.setPositionEased(tappedPoint, 1000);

        return true;
    }

    @Override
    public boolean onDoubleTap(float x, float y) {
        map.setZoomEased(map.getZoom() + 1.f, 500);
        LngLat tapped = map.coordinatesAtScreenPosition(x, y);
        LngLat current = map.getPosition();
        LngLat next = new LngLat(
                .5 * (tapped.longitude + current.longitude),
                .5 * (tapped.latitude + current.latitude));
        map.setPositionEased(next, 500);
        return true;
    }

    @Override
    public void onLongPress(float x, float y) {
        // markers.clear();
        // showTileInfo = !showTileInfo;
        // map.setDebugFlag(MapController.DebugFlag.TILE_INFOS, showTileInfo);

        Toast.makeText(getApplicationContext(), "Take Screenshot",
                       Toast.LENGTH_SHORT).show();

        map.captureFrame(new FrameCaptureCallback() {
                public void onCaptured(final Bitmap bitmap) {
                    runOnUiThread(new Runnable() {
                            public void run() { saveScreenshot(bitmap); }
                        });
                }
            }, true);
    }

    @Override
    public void onFeaturePick(Map<String, String> properties, float positionX, float positionY) {
        String name = properties.get("name");
        if (name.isEmpty()) {
            name = "unnamed";
        }
        Toast.makeText(getApplicationContext(),
                "Selected: " + name + " at: " + positionX + ", " + positionY,
                Toast.LENGTH_SHORT).show();
    }

    private void saveScreenshot(Bitmap bitmap) {


        final Calendar c = Calendar.getInstance();
        String fileName = "img" + String.valueOf(c.getTimeInMillis()) + ".png";

        File dirName = new File(Environment.getExternalStorageDirectory() +
                                File.separator + "Pictures"+
                                File.separator + "Screenshots");
        dirName.mkdirs();

        FileOutputStream fos = null;
        try {
            File tmpFile = new File(dirName, fileName);
            fos = new FileOutputStream(tmpFile);

            bitmap.compress(Bitmap.CompressFormat.PNG, 90, fos);
            Toast.makeText(getApplicationContext(),
                           "Took Screenshot " + tmpFile,
                           Toast.LENGTH_SHORT).show();
            return;

        } catch (FileNotFoundException e) {
            e.printStackTrace();
            Toast.makeText(getApplicationContext(),
                           "Took Screenshot: " + e,
                           Toast.LENGTH_SHORT).show();
        } finally {
            if (fos != null) {
                try { fos.close(); }
                catch (IOException e) {}
            }
        }

    }

}

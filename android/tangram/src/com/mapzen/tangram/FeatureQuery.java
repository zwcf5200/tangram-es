package com.mapzen.tangram;

import android.support.annotation.Nullable;

import java.util.Map;

public abstract class FeatureQuery {

    public FeatureQuery(float screenX, float screenY) {
        this.x = screenX;
        this.y = screenY;
    }

    public float getX() {
        return x;
    }

    public float getY() {
        return y;
    }

    public abstract void onQueryComplete(@Nullable FeatureResult result);

    private float x;
    private float y;

    void resultFromNative(Map<String, String> properties, int type, double lng, double lat) {
        FeatureResult result = null;

        if (properties != null) {
            FeatureLabel label = null;
            if (type >= 0) {
                label = new FeatureLabel(lng, lat, type);
            }
            result = new FeatureResult(properties, label);
        }

        onQueryComplete(result);
    }
}

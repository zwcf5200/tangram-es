package com.mapzen.tangram;

public class FeatureLabel {

    public enum Type {
        ICON,
        TEXT,
    }

    FeatureLabel(double lng, double lat, int type) {
        this.coordinates = new LngLat(lng, lat);
        this.type = Type.values()[type];
    }

    public LngLat getCoordinates() {
        return coordinates;
    }

    public Type getType() {
        return type;
    }

    private LngLat coordinates;
    private Type type;
}

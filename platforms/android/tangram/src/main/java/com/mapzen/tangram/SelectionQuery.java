package com.mapzen.tangram;

import java.util.Map;

class SelectionQuery {

    public enum Type {
        LABEL,
        FEATURE,
        MARKER,
    }

    private LabelPickResult labelPickResult;
    private Map<String, String> featurePickResult;
    private MarkerPickResult markerPickresult;

    private float x;
    private float y;
    private Type type;

    public SelectionQuery(float x, float y, Type type) {
        this.x = x;
        this.y = y;
        this.type = type;
    }

    public float getX() {
        return this.x;
    }

    public float getY() {
        return this.y;
    }

    public float[] getPosition() {
        float[] position = new float[]{ x, y };
        return position;
    }

    public int getTypeOrdinal() {
        return this.type.ordinal();
    }

    public Type getType() {
        return this.type;
    }

    public void setLabelPickResult(LabelPickResult labelPickResult) {
        this.labelPickResult = labelPickResult;
    }

    public void setFeaturePickResult(Map<String, String> featurePickResult) {
        this.featurePickResult = featurePickResult;
    }

    public void setMarkerPickResult(MarkerPickResult markerPickresult) {
        this.markerPickresult = markerPickresult;
    }

    public LabelPickResult getLabelPickResult() {
        return this.labelPickResult;
    }

    public Map<String, String> getFeaturePickResult() {
        return this.featurePickResult;
    }

    public MarkerPickResult getMarkerPickresult() {
        return this.markerPickresult;
    }
}

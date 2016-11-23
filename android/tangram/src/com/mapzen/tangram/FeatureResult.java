package com.mapzen.tangram;

import java.util.Map;

public class FeatureResult {

    FeatureResult(Map<String, String> properties, FeatureLabel label) {
        this.properties = properties;
        this.label = label;
    }

    public Map<String, String> getProperties() {
        return properties;
    }

    public FeatureLabel getLabel() {
        return label;
    }

    private Map<String, String> properties;
    private FeatureLabel label;
}

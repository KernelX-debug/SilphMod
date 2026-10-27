package dev.rootcatch.ugphone;

import java.util.Locale;

public final class KecleonMovement {
    public static final long STEP_MS=350;
    static final double RADIUS_METERS=8;
    public static double[][] points(double latitude,double longitude){
        if(!Double.isFinite(latitude)||!Double.isFinite(longitude)||Math.abs(latitude)>89||Math.abs(longitude)>180)
            throw new IllegalArgumentException("Invalid movement coordinates");
        double delta=Math.toDegrees(RADIUS_METERS/6371008.8),offset=delta/Math.cos(Math.toRadians(latitude)),east=longitude+offset,west=longitude-offset;
        if(east>180)east-=360;
        if(west< -180)west+=360;
        return new double[][]{{latitude,longitude},{latitude,east},{latitude+delta,longitude},
                {latitude,west},{latitude-delta,longitude},{latitude,longitude}};
    }
    public static String command(double latitude,double longitude){
        if(!Double.isFinite(latitude)||!Double.isFinite(longitude)||Math.abs(latitude)>90||Math.abs(longitude)>180)
            throw new IllegalArgumentException("Invalid teleport coordinates");
        return String.format(Locale.ROOT,"am start-foreground-service -a theappninjas.gpsjoystick.TELEPORT --ef lat %.8f --ef lng %.8f",latitude,longitude);
    }
    private KecleonMovement(){ }
}

package dev.rootcatch.ugphone;

import android.content.Context;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Locale;

final class KecleonHistory extends SQLiteOpenHelper {
    KecleonHistory(Context context){super(context,"kecleon-history.db",null,2);}
    @Override public void onCreate(SQLiteDatabase db){
        db.execSQL("CREATE TABLE candidates(account TEXT NOT NULL, candidate TEXT NOT NULL, state TEXT NOT NULL, updated INTEGER NOT NULL, attempts INTEGER NOT NULL DEFAULT 1, PRIMARY KEY(account,candidate))");
        db.execSQL("CREATE TABLE incidents(account TEXT NOT NULL, incident TEXT NOT NULL, candidate TEXT NOT NULL, PRIMARY KEY(account,incident))");
    }
    @Override public void onUpgrade(SQLiteDatabase db,int oldVersion,int newVersion){
        if(oldVersion==1&&newVersion==2)db.execSQL("ALTER TABLE candidates ADD COLUMN attempts INTEGER NOT NULL DEFAULT 1");
        else throw new IllegalStateException("Unknown candidate history version");
    }
    static String hash(String value){
        try {byte[] bytes=MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
            StringBuilder text=new StringBuilder();for(byte b:bytes)text.append(String.format(Locale.ROOT,"%02x",b&255));return text.toString();
        }catch(Exception error){throw new IllegalStateException(error);}
    }
    static String key(KecleonFeed.Candidate value){
        return hash("moonani-kecleon\n"+value.id+"\n"+value.coordinates+"\n"+value.start+"\n"+value.end);
    }
    synchronized boolean claim(String account,String candidate){
        SQLiteDatabase db=getWritableDatabase();db.beginTransaction();
        try{
            int attempts=1;boolean exists=false;long now=System.currentTimeMillis();
            try(Cursor cursor=db.query("candidates",new String[]{"state","attempts","updated"},"account=? AND candidate=?",new String[]{account,candidate},null,null,null)){
                if(cursor.moveToFirst()){
                    exists=true;
                    if(KecleonRetryPolicy.retryAt(cursor.getString(0),cursor.getInt(1),cursor.getLong(2))>now){db.setTransactionSuccessful();return false;}
                    attempts=cursor.getInt(1)+1;
                }
            }
            ContentValues values=new ContentValues();values.put("state","claimed");values.put("updated",now);values.put("attempts",attempts);
            if(exists)db.update("candidates",values,"account=? AND candidate=?",new String[]{account,candidate});
            else {values.put("account",account);values.put("candidate",candidate);db.insertOrThrow("candidates",null,values);}
            db.setTransactionSuccessful();return true;
        }finally{db.endTransaction();}
    }
    synchronized boolean incident(String account,String fort,String incident,String candidate){
        String key=hash(fort+"\n"+incident);SQLiteDatabase db=getWritableDatabase();db.beginTransaction();
        try{
            try(Cursor cursor=db.query("incidents",new String[]{"candidate"},"account=? AND incident=?",new String[]{account,key},null,null,null)){
                if(cursor.moveToFirst()){boolean same=cursor.getString(0).equals(candidate);db.setTransactionSuccessful();return same;}
            }
            ContentValues values=new ContentValues();values.put("account",account);values.put("incident",key);values.put("candidate",candidate);
            db.insertOrThrow("incidents",null,values);db.setTransactionSuccessful();return true;
        }finally{db.endTransaction();}
    }
    synchronized long retryAt(String account,String candidate){
        try(Cursor cursor=getReadableDatabase().query("candidates",new String[]{"state","attempts","updated"},"account=? AND candidate=?",new String[]{account,candidate},null,null,null)){
            return cursor.moveToFirst()?KecleonRetryPolicy.retryAt(cursor.getString(0),cursor.getInt(1),cursor.getLong(2)):Long.MAX_VALUE;
        }
    }
    synchronized void finish(String account,String candidate,String state,boolean verified){
        ContentValues values=new ContentValues();values.put("state",verified?state:"unverified:"+state);values.put("updated",System.currentTimeMillis());
        if(getWritableDatabase().update("candidates",values,"account=? AND candidate=?",new String[]{account,candidate})!=1)
            throw new IllegalStateException("Candidate history missing");
    }
    synchronized void clearAll(){
        SQLiteDatabase db=getWritableDatabase();db.beginTransaction();
        try{db.delete("candidates",null,null);db.delete("incidents",null,null);db.setTransactionSuccessful();}finally{db.endTransaction();}
    }
}

package dev.silphmod.nativecheck;

import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.system.Os;
import android.system.OsConstants;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

final class CheckLog {
    private static final long MAX_BYTES=8*1024*1024,FINAL_RESERVE=256*1024;
    private static final Object FILE_LOCK=new Object();
    private static volatile CheckLog active;
    private final File file;
    private long bytes,dropped;
    private String writeError="";
    CheckLog(Context context) throws IOException {
        File folder=new File(context.getFilesDir(),SessionLogFiles.CURRENT);
        if(!folder.isDirectory()&&!folder.mkdirs())throw new IOException("Cannot create session log");
        file=new File(folder,"session-"+System.currentTimeMillis()+"_"+UUID.randomUUID().toString().substring(0,8)+".jsonl");
        synchronized(FILE_LOCK){
            if(!file.createNewFile())throw new IOException("Session log already exists");
            append("build",device(context).toString());
            active=this;
        }
        context.getSharedPreferences("check",0).edit().putString("log",file.getAbsolutePath()).apply();
    }
    private void append(String event,String message) throws IOException {
        try{
            JSONObject row=new JSONObject().put("time",System.currentTimeMillis()).put("event",event).put("message",LogPrivacy.redact(message));
            byte[] data=(row+"\n").getBytes(StandardCharsets.UTF_8);
            boolean terminal=event.equals("finished")||event.equals("error")||event.startsWith("startup_")||event.startsWith("failure_")||event.equals("native_cleanup")||event.equals("native_failure_status");
            if(bytes+data.length>(terminal?MAX_BYTES:MAX_BYTES-FINAL_RESERVE)){dropped++;return;}
            try(FileOutputStream out=new FileOutputStream(file,true)){out.write(data);}
            bytes+=data.length;
        }catch(org.json.JSONException error){throw new IOException("Cannot encode log event",error);}
    }
    void write(String event,String message){
        synchronized(FILE_LOCK){try{append(event,message);}catch(IOException error){writeError=error.toString();dropped++;}}
    }
    static JSONObject device(Context context){
        JSONObject result=new JSONObject();
        try{
            result.put("version",BuildConfig.VERSION_NAME).put("versionCode",BuildConfig.VERSION_CODE).put("package",BuildConfig.APPLICATION_ID)
                .put("android",Build.VERSION.RELEASE).put("sdk",Build.VERSION.SDK_INT).put("manufacturer",Build.MANUFACTURER)
                .put("model",Build.MODEL).put("fingerprint",Build.FINGERPRINT).put("abis",new JSONArray(java.util.Arrays.asList(Build.SUPPORTED_ABIS)))
                .put("nativeLibraryDir",context.getApplicationInfo().nativeLibraryDir);
            try{result.put("pageSize",Os.sysconf(OsConstants._SC_PAGESIZE));}catch(Exception error){result.put("pageSizeError",error.toString());}
            try{PackageInfo game=context.getPackageManager().getPackageInfo("com.nianticlabs.pokemongo",0);result.put("gameVersion",game.versionName).put("gameVersionCode",game.getLongVersionCode());}catch(Exception error){result.put("gameError",error.toString());}
        }catch(Exception ignored){ }
        return result;
    }
    private static void json(ZipOutputStream zip,String name,JSONObject value) throws IOException {
        zip.putNextEntry(new ZipEntry(name));zip.write((value+"\n").getBytes(StandardCharsets.UTF_8));zip.closeEntry();
    }
    static Uri export(Context context) throws Exception {
        String pointer=context.getSharedPreferences("check",0).getString("log","");
        List<File> files=SessionLogFiles.latest(context.getFilesDir(),pointer,3);
        ContentValues values=new ContentValues();
        String stamp=new SimpleDateFormat("yyyyMMdd_HHmmss",Locale.US).format(new Date());
        values.put(MediaStore.Downloads.DISPLAY_NAME,"SilphMod_"+stamp+"_"+UUID.randomUUID().toString().substring(0,8)+".zip");
        values.put(MediaStore.Downloads.MIME_TYPE,"application/zip");values.put(MediaStore.Downloads.RELATIVE_PATH,"Download/SilphMod");values.put(MediaStore.Downloads.IS_PENDING,1);
        Uri uri=context.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,values);
        if(uri==null)throw new IOException("Cannot create export");
        try{
            OutputStream stream=context.getContentResolver().openOutputStream(uri);
            if(stream==null)throw new IOException("Cannot open export");
            try(ZipOutputStream zip=new ZipOutputStream(stream)){
                json(zip,"build.json",device(context));
                JSONArray manifest=new JSONArray();
                for(File file:files){
                    String entry=file.getParentFile().getName()+"/"+file.getName();
                    long sourceBytes;synchronized(FILE_LOCK){sourceBytes=file.length();}
                    long remaining=Math.min(sourceBytes,MAX_BYTES),written=0;
                    MessageDigest digest=MessageDigest.getInstance("SHA-256");
                    zip.putNextEntry(new ZipEntry(entry));
                    try(InputStream in=new FileInputStream(file)){
                        byte[] buffer=new byte[32768];int count;
                        while(remaining>0&&(count=in.read(buffer,0,(int)Math.min(buffer.length,remaining)))!=-1){zip.write(buffer,0,count);digest.update(buffer,0,count);remaining-=count;written+=count;}
                    }
                    zip.closeEntry();
                    StringBuilder hash=new StringBuilder();for(byte b:digest.digest())hash.append(String.format(Locale.US,"%02x",b&255));
                    manifest.put(new JSONObject().put("path",entry).put("sourceBytes",sourceBytes).put("exportedBytes",written).put("sha256",hash.toString()));
                }
                JSONObject status=new JSONObject().put("exportedAt",System.currentTimeMillis()).put("sessions",manifest).put("sessionCount",files.size());
                synchronized(FILE_LOCK){CheckLog current=active;if(current!=null)status.put("loggingHealth",new JSONObject().put("file",current.file.getName()).put("bytes",current.bytes).put("droppedEvents",current.dropped).put("writeError",current.writeError));}
                if(files.isEmpty())status.put("status","No saved sessions; device information exported");
                json(zip,"manifest.json",status);
            }
            ContentValues ready=new ContentValues();ready.put(MediaStore.Downloads.IS_PENDING,0);
            if(context.getContentResolver().update(uri,ready,null,null)<1)throw new IOException("Cannot finalize export");return uri;
        }catch(Exception error){context.getContentResolver().delete(uri,null,null);throw error;}
    }
}

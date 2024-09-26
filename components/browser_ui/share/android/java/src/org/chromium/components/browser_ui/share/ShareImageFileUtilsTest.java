// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Environment;
import android.os.Looper;
import android.provider.MediaStore;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests of {@link ShareImageFileUtils}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareImageFileUtilsTest extends BlankUiTestActivityTestCase {
    private static final long WAIT_TIMEOUT_SECONDS = 30L;
    private static final byte[] TEST_IMAGE_DATA = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    private static final String TEST_IMAGE_FILE_NAME = "chrome-test-bitmap";
    private static final String TEST_GIF_IMAGE_FILE_EXTENSION = ".gif";
    private static final String TEST_JPG_IMAGE_FILE_EXTENSION = ".jpg";
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    private class GenerateUriCallback extends CallbackHelper implements Callback<Uri> {
        private Uri mImageUri;

        public Uri getImageUri() {
            return mImageUri;
        }

        @Override
        public void onResult(Uri uri) {
            mImageUri = uri;
            notifyCalled();
        }
    }

    private class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        Looper.prepare();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());
    }

    @Override
    public void tearDownTest() throws Exception {
        Clipboard.getInstance().setText("");
        clearSharedImages();
        deleteAllTestImages();
        super.tearDownTest();
    }

    private int fileCount(File file) {
        if (file.isFile()) {
            return 1;
        }

        int count = 0;
        if (file.isDirectory()) {
            for (File f : file.listFiles()) count += fileCount(f);
        }
        return count;
    }

    private boolean filepathExists(File file, String filepath) {
        if (file.isFile() && filepath.endsWith(file.getName())) {
            return true;
        }

        if (file.isDirectory()) {
            for (File f : file.listFiles()) {
                if (filepathExists(f, filepath)) return true;
            }
        }
        return false;
    }

    private Uri generateAnImageToClipboard(String fileExtension) throws TimeoutException {
        GenerateUriCallback imageCallback = new GenerateUriCallback();
        ShareImageFileUtils.generateTemporaryUriFromData(
                TEST_IMAGE_DATA, fileExtension, imageCallback);
        imageCallback.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Clipboard.getInstance().setImageUri(imageCallback.getImageUri());
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(Clipboard.getInstance().getImageUri(),
                    Matchers.is(imageCallback.getImageUri()));
        });
        return imageCallback.getImageUri();
    }

    private Uri generateAnImageToClipboard() throws TimeoutException {
        return generateAnImageToClipboard(TEST_JPG_IMAGE_FILE_EXTENSION);
    }

    private void clearSharedImages() throws TimeoutException {
        ShareImageFileUtils.clearSharedImages();

        // ShareImageFileUtils::clearSharedImages uses AsyncTask.SERIAL_EXECUTOR to schedule a
        // clearing the shared folder job, so schedule a new job and wait for the new job finished
        // to make sure ShareImageFileUtils::clearSharedImages's clearing folder job finished.
        waitForAsync();
    }

    private void waitForAsync() throws TimeoutException {
        AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
        AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        AsyncTask.THREAD_POOL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void deleteAllTestImages() throws TimeoutException {
        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                deleteMediaStoreFiles();
            }
            deleteExternalStorageFiles();
        });
        waitForAsync();
    }

    @RequiresApi(29)
    private void deleteMediaStoreFiles() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        Cursor cursor =
                contentResolver.query(MediaStore.Downloads.EXTERNAL_CONTENT_URI, null, null, null);
        while (cursor.moveToNext()) {
            long id = cursor.getLong(cursor.getColumnIndexOrThrow(MediaStore.Downloads._ID));
            Uri uri = ContentUris.withAppendedId(MediaStore.Downloads.EXTERNAL_CONTENT_URI, id);
            contentResolver.delete(uri, null, null);
        }
    }

    public void deleteExternalStorageFiles() {
        File externalStorageDir = ContextUtils.getApplicationContext().getExternalFilesDir(
                Environment.DIRECTORY_DOWNLOADS);
        String[] children = externalStorageDir.list();
        for (int i = 0; i < children.length; i++) {
            new File(externalStorageDir, children[i]).delete();
        }
    }

    private int fileCountInShareDirectory() throws IOException {
        return fileCount(ShareImageFileUtils.getSharedFilesDirectory());
    }

    private boolean fileExistsInShareDirectory(Uri fileUri) throws IOException {
        return filepathExists(ShareImageFileUtils.getSharedFilesDirectory(), fileUri.getPath());
    }

    private Bitmap getTestBitmap() {
        int size = 10;
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColor(android.graphics.Color.GREEN);
        canvas.drawRect(0F, 0F, (float) size, (float) size, paint);
        return bitmap;
    }

    @Test
    @SmallTest
    public void clipboardUriDoNotClearTest() throws TimeoutException, IOException {
        Uri clipboardUri = generateAnImageToClipboard(TEST_GIF_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_GIF_IMAGE_FILE_EXTENSION));
        clipboardUri = generateAnImageToClipboard(TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_JPG_IMAGE_FILE_EXTENSION));
        clipboardUri = generateAnImageToClipboard(TEST_PNG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(clipboardUri.getPath().endsWith(TEST_PNG_IMAGE_FILE_EXTENSION));
        Assert.assertEquals(3, fileCountInShareDirectory());

        clearSharedImages();
        Assert.assertEquals(1, fileCountInShareDirectory());
        Assert.assertTrue(fileExistsInShareDirectory(clipboardUri));
    }

    @Test
    @SmallTest
    public void clearEverythingIfNoClipboardImageTest() throws TimeoutException, IOException {
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        Assert.assertEquals(3, fileCountInShareDirectory());

        Clipboard.getInstance().setText("");
        clearSharedImages();
        Assert.assertEquals(0, fileCountInShareDirectory());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1056059")
    public void testSaveBitmap() throws IOException, TimeoutException {
        String fileName = TEST_IMAGE_FILE_NAME + "_save_bitmap";
        ShareImageFileUtils.OnImageSaveListener listener =
                new ShareImageFileUtils.OnImageSaveListener() {
                    @Override
                    public void onImageSaved(Uri uri, String displayName) {
                        Assert.assertNotNull(uri);
                        Assert.assertEquals(fileName, displayName);
                        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
                            File file = new File(uri.getPath());
                            Assert.assertTrue(file.exists());
                            Assert.assertTrue(file.isFile());
                        });

                        // Wait for the above checks to complete.
                        try {
                            waitForAsync();
                        } catch (TimeoutException ex) {
                        }
                    }

                    @Override
                    public void onImageSaveError(String displayName) {
                        Assert.fail();
                    }
                };
        ShareImageFileUtils.saveBitmapToExternalStorage(
                getActivity(), fileName, getTestBitmap(), listener);
        waitForAsync();
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(value = VERSION_CODES.Q, reason = "Added to completed downloads on P-")
    public void testSaveBitmapAndMediaStore() throws IOException, TimeoutException {
        String fileName = TEST_IMAGE_FILE_NAME + "_mediastore";
        ShareImageFileUtils.OnImageSaveListener listener =
                new ShareImageFileUtils.OnImageSaveListener() {
                    @Override
                    public void onImageSaved(Uri uri, String displayName) {
                        Assert.assertNotNull(uri);
                        Assert.assertEquals(fileName, displayName);
                        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
                            Cursor cursor = getActivity().getContentResolver().query(
                                    uri, null, null, null, null);
                            Assert.assertNotNull(cursor);
                            Assert.assertTrue(cursor.moveToFirst());
                            Assert.assertEquals(fileName + TEST_JPG_IMAGE_FILE_EXTENSION,
                                    cursor.getString(cursor.getColumnIndex(
                                            MediaStore.MediaColumns.DISPLAY_NAME)));
                        });

                        // Wait for the above checks to complete.
                        try {
                            waitForAsync();
                        } catch (TimeoutException ex) {
                        }
                    }

                    @Override
                    public void onImageSaveError(String displayName) {
                        Assert.fail();
                    }
                };
        ShareImageFileUtils.saveBitmapToExternalStorage(
                getActivity(), fileName, getTestBitmap(), listener);
        waitForAsync();
    }

    @Test
    @SmallTest
    public void testGetNextAvailableFile() throws IOException {
        String fileName = TEST_IMAGE_FILE_NAME + "_next_availble";
        File externalStorageDir =
                getActivity().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        File imageFile = ShareImageFileUtils.getNextAvailableFile(
                externalStorageDir.getPath(), fileName, TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(imageFile.exists());

        File imageFile2 = ShareImageFileUtils.getNextAvailableFile(
                externalStorageDir.getPath(), fileName, TEST_JPG_IMAGE_FILE_EXTENSION);
        Assert.assertTrue(imageFile2.exists());
        Assert.assertNotEquals(imageFile.getPath(), imageFile2.getPath());
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(value = VERSION_CODES.P, reason = "Added to MediaStore.Downloads on Q+")
    public void testAddCompletedDownload() throws IOException {
        String filename =
                TEST_IMAGE_FILE_NAME + "_add_completed_download" + TEST_JPG_IMAGE_FILE_EXTENSION;
        File externalStorageDir =
                getActivity().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        File qrcodeFile = new File(externalStorageDir, filename);
        Assert.assertTrue(qrcodeFile.createNewFile());

        long downloadId = ShareImageFileUtils.addCompletedDownload(qrcodeFile);
        Assert.assertNotEquals(0L, downloadId);

        DownloadManager downloadManager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        DownloadManager.Query query = new DownloadManager.Query().setFilterById(downloadId);
        Cursor c = downloadManager.query(query);

        Assert.assertNotNull(c);
        Assert.assertTrue(c.moveToFirst());
        Assert.assertEquals(
                filename, c.getString(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TITLE)));
        c.close();
    }
}

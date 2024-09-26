// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Assists in Java interaction the native Sync FakeServer. Can be used from any thread.
 */
public class FakeServerHelper {
    private static final String TAG = "FakeServerHelper";

    // Singleton instance. Set on every createInstanceAndGet() and reset to null on every destroy().
    // This must be assigned on the UI thread.
    private static FakeServerHelper sFakeServerHelper;

    // Must be used from the UI thread.
    private final long mNativeFakeServer;

    /**
     * Creates the singleton FakeServerHelper and returns it. destroyInstance() must be called
     * when done to prevent the native object from leaking. If this is called before the previous
     * instance is destroyed, it will return null (just to avoid throwing ExecutionException).
     * TODO(crbug.com/949504): When refactoring this method, throw an exception instead of returning
     * null.
     */
    public static @Nullable FakeServerHelper createInstanceAndGet() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            if (sFakeServerHelper == null) {
                sFakeServerHelper = new FakeServerHelper();
                return sFakeServerHelper;
            }

            Log.w(TAG,
                    "destroyInstance() must be called before another FakeServerHelper is created");
            return null;
        });
    }

    /**
     * Deletes the existing FakeServer if any.
     */
    public static void destroyInstance() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (sFakeServerHelper == null) return;

            FakeServerHelperJni.get().deleteFakeServer(sFakeServerHelper.mNativeFakeServer);
            sFakeServerHelper = null;
        });
    }

    private FakeServerHelper() {
        ThreadUtils.assertOnUiThread();
        mNativeFakeServer = FakeServerHelperJni.get().createFakeServer();
        assert mNativeFakeServer != 0L;
    }

    /**
     * Returns whether {@code count} entities exist on the fake Sync server with the given
     * {@code modelType} and {@code name}.
     *
     * @param count the number of fake server entities to verify
     * @param modelType the model type of entities to verify
     * @param name the name of entities to verify
     *
     * @return whether the number of specified entities exist
     */
    public boolean verifyEntityCountByTypeAndName(
            final int count, final int modelType, final String name) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> FakeServerHelperJni.get().verifyEntityCountByTypeAndName(
                                mNativeFakeServer, count, modelType, name));
    }

    /**
     * Verifies whether the sessions on the fake Sync server match the given set of urls.
     *
     * @param urls the set of urls to check against; order does not matter.
     *
     * @return whether the sessions on the server match the given urls.
     */
    public boolean verifySessions(final String[] urls) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> FakeServerHelperJni.get().verifySessions(mNativeFakeServer, urls));
    }

    /**
     * Returns all the SyncEntities on the fake server with the given modelType.
     *
     * @param modelType the type of entities to return.
     *
     * @return a list of all the SyncEntity protos for that type.
     */
    public List<SyncEntity> getSyncEntitiesByModelType(final int modelType)
            throws InvalidProtocolBufferException {
        byte[][] serializedEntities = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> FakeServerHelperJni.get().getSyncEntitiesByModelType(
                                mNativeFakeServer, modelType));
        List<SyncEntity> entities = new ArrayList<SyncEntity>(serializedEntities.length);
        for (byte[] serializedEntity : serializedEntities) {
            entities.add(SyncEntity.parseFrom(serializedEntity));
        }
        return entities;
    }

    /**
     * Injects an entity into the fake Sync server. This method only works for entities that will
     * eventually contain a unique client tag (e.g., preferences, typed URLs).
     *
     * @param nonUniqueName the human-readable name for the entity. This value will be used for the
     *        SyncEntity.name value
     * @param clientTag the ID that makes this entity unique across clients. This value will be used
     *        in hashed form in SyncEntity.server_defined_unique_tag
     * @param entitySpecifics the EntitySpecifics proto that represents the entity to inject
     */
    public void injectUniqueClientEntity(final String nonUniqueName, final String clientTag,
            final EntitySpecifics entitySpecifics) {
        // The protocol buffer is serialized as a byte array because it can be easily
        // deserialized from this format in native code.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().injectUniqueClientEntity(mNativeFakeServer,
                                nonUniqueName, clientTag, entitySpecifics.toByteArray()));
    }

    /**
     * Sets the Wallet card and address data to be served in following GetUpdates requests. Note
     * that (opposed to the native implementation) this currently only accepts a single entity,
     * because that's all we needed so far.
     *
     * @param entity the SyncEntity to serve for Wallet.
     */
    public void setWalletData(final SyncEntity entity) {
        // The protocol buffer is serialized as a byte array because it can be easily
        // deserialized from this format in native code.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().setWalletData(
                                mNativeFakeServer, entity.toByteArray()));
    }

    /**
     * Modify the specifics of an entity on the fake Sync server.
     *
     * @param id the ID of the entity whose specifics to modify
     * @param entitySpecifics the new specifics proto for the entity
     */
    public void modifyEntitySpecifics(final String id, final EntitySpecifics entitySpecifics) {
        // The protocol buffer is serialized as a byte array because it can be easily
        // deserialized from this format in native code.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().modifyEntitySpecifics(
                                mNativeFakeServer, id, entitySpecifics.toByteArray()));
    }

    /**
     * Injects a bookmark into the fake Sync server.
     *
     * @param title the title of the bookmark to inject
     * @param url the URL of the bookmark to inject. This String will be passed to the native GURL
     *            class, so it must be a valid URL under its definition.
     * @param parentId the ID of the desired parent bookmark folder
     * @param parentGuid the GUID of the desired parent bookmark folder
     */
    public void injectBookmarkEntity(
            final String title, final GURL url, final String parentId, final String parentGuid) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().injectBookmarkEntity(
                                mNativeFakeServer, title, url, parentId, parentGuid));
    }

    /**
     * Injects a bookmark folder into the fake Sync server.
     *
     * @param title the title of the bookmark folder to inject
     * @param parentId the ID of the desired parent bookmark folder
     * @param parentGuid the GUID of the desired parent bookmark folder
     */
    public void injectBookmarkFolderEntity(
            final String title, final String parentId, final String parentGuid) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().injectBookmarkFolderEntity(
                                mNativeFakeServer, title, parentId, parentGuid));
    }

    /**
     * Modifies an existing bookmark on the fake Sync server.
     *
     * @param bookmarkId the ID of the bookmark to modify
     * @param bookmarkGuid the GUID of the bookmark to modify
     * @param title the new title of the bookmark
     * @param url the new URL of the bookmark. This String will be passed to the native GURL
     *            class, so it must be a valid URL under its definition.
     * @param parentId the ID of the new desired parent bookmark folder
     * @param parentGuid the GUID of the new desired parent bookmark folder
     */
    public void modifyBookmarkEntity(final String bookmarkId, final String bookmarkGuid,
            final String title, final GURL url, final String parentId, final String parentGuid) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().modifyBookmarkEntity(mNativeFakeServer,
                                bookmarkId, bookmarkGuid, title, url, parentId, parentGuid));
    }

    /**
     * Modifies an existing bookmark folder on the fake Sync server.
     *
     * @param folderId the ID of the bookmark folder to modify
     * @param folderGuid the GUID of the bookmark folder to modify
     * @param title the new title of the bookmark folder
     * @param parentId the ID of the new desired parent bookmark folder
     * @param parentGuid the GUID of the new desired parent bookmark folder
     */
    public void modifyBookmarkFolderEntity(final String folderId, final String folderGuid,
            final String title, final String parentId, final String parentGuid) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().modifyBookmarkFolderEntity(mNativeFakeServer,
                                folderId, folderGuid, title, parentId, parentGuid));
    }

    /**
     * Deletes an entity on the fake Sync server.
     *
     * In other words, this method injects a tombstone into the fake Sync server.
     *
     * @param id the server ID of the entity to delete
     * @param clientTagHash the client defined unique tag hash of the entity to delete (or an empty
     *         string if sync does not care about this being a hash)
     */
    public void deleteEntity(final String id) {
        deleteEntity(id, "");
    }

    public void deleteEntity(final String id, final String clientTagHash) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> FakeServerHelperJni.get().deleteEntity(mNativeFakeServer, id, clientTagHash));
    }

    /**
     * Returns the ID of the Bookmark Bar. This value is to be used in conjunction with
     * injectBookmarkEntity.
     *
     * @return the opaque ID of the bookmark bar entity stored in the server
     */
    public String getBookmarkBarFolderId() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> FakeServerHelperJni.get().getBookmarkBarFolderId(mNativeFakeServer));
    }

    /**
     * Sets trusted vault nigori with keys derived from trustedVaultKey on the server.
     */
    public void setTrustedVaultNigori(byte[] trustedVaultKey) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> FakeServerHelperJni.get().setTrustedVaultNigori(
                                mNativeFakeServer, trustedVaultKey));
    }

    /**
     * Clear the server data (perform dashboard stop and clear).
     */
    public void clearServerData() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> FakeServerHelperJni.get().clearServerData(mNativeFakeServer));
    }

    @NativeMethods
    interface Natives {
        long createFakeServer();
        void deleteFakeServer(long fakeServer);
        boolean verifyEntityCountByTypeAndName(
                long fakeServer, int count, int modelType, String name);
        boolean verifySessions(long fakeServer, String[] urlArray);
        byte[][] getSyncEntitiesByModelType(long fakeServer, int modelType);
        void injectUniqueClientEntity(long fakeServer, String nonUniqueName, String clientTag,
                byte[] serializedEntitySpecifics);
        void setWalletData(long fakeServer, byte[] serializedEntity);
        void modifyEntitySpecifics(long fakeServer, String id, byte[] serializedEntitySpecifics);
        void injectBookmarkEntity(
                long fakeServer, String title, GURL url, String parentId, String parentGuid);
        void injectBookmarkFolderEntity(
                long fakeServer, String title, String parentId, String parentGuid);
        void modifyBookmarkEntity(long fakeServer, String bookmarkId, String bookmarkGuid,
                String title, GURL url, String parentId, String parentGuid);
        void modifyBookmarkFolderEntity(long fakeServer, String bookmarkId, String bookmarkGuid,
                String title, String parentId, String parentGuid);
        String getBookmarkBarFolderId(long fakeServer);
        void deleteEntity(long fakeServer, String id, String clientDefinedUniqueTag);
        void setTrustedVaultNigori(long fakeServer, byte[] trustedVaultKey);
        void clearServerData(long fakeServer);
    }
}

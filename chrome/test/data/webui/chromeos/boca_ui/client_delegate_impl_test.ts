// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClientDelegateFactory, getNetworkInfoMojomToUI, getSessionConfigMojomToUI, getStudentActivityMojomToUI} from 'chrome-untrusted://boca-app/app/client_delegate.js';
import {CaptionConfig, Config, Course, Identity, OnTaskConfig, PageHandlerRemote, RemoveStudentError, SessionResult, SubmitAccessCodeError, UpdateSessionError, Window} from 'chrome-untrusted://boca-app/mojom/boca.mojom-webui.js';
import {Url} from 'chrome-untrusted://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertDeepEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class MockRemoteHandler extends PageHandlerRemote {
  override getWindowsTabsList(): Promise<{windowList: Window[]}> {
    const url1 = new Url();
    url1.url = 'http://foo1';
    const url2 = new Url();
    url2.url = 'http://foo2';
    const url3 = new Url();
    url3.url = 'http://foo3';
    return Promise.resolve({
      windowList: [
        {
          name: 'window1',
          tabList: [
            {title: 'title1', url: url1, favicon: 'dataurl1'},
            {title: 'title2', url: url2, favicon: 'dataurl2'},
          ],
        },
        {tabList: [{title: 'title3', url: url3, favicon: 'dataurl3'}]},
      ] as Window[],
    });
  }
  override listCourses(): Promise<{courses: Course[]}> {
    return Promise.resolve(
        {courses: [{id: '1', name: 'course1'}, {id: '2', name: 'course2'}]});
  }
  override listStudents(id: string): Promise<{students: Identity[]}> {
    // Dummy action get around with unused variable check.
    id;
    return Promise.resolve({
      students: [
        {id: '1', name: 'cat', email: 'email1', photoUrl: {url: 'cdn1'}},
        {id: '2', name: 'dog', email: 'email2', photoUrl: {url: 'cdn2'}},
      ],
    });
  }

  override createSession(config: Config): Promise<{success: boolean}> {
    assertDeepEquals(
        {
          sessionDuration: {
            // BigInt serialized as string.
            microseconds: 7200000000n,
          },
          students: [
            {
              id: '1',
              name: 'cat',
              email: 'cat@gmail.com',
              photoUrl: {url: 'cdn1'},
            },
            {
              id: '2',
              name: 'dog',
              email: 'dog@gmail.com',
              photoUrl: {url: 'cdn2'},
            },
          ],
          studentsJoinViaCode: [],
          teacher: null,
          accessCode: null,
          sessionStartTime: null,
          onTaskConfig: {
            isLocked: true,
            tabs: [
              {
                tab: {
                  url: {url: 'http://google.com/'},
                  title: 'google',
                  favicon: 'data/image',
                },
                navigationType: 0,
              },
              {
                tab: {
                  url: {url: 'http://youtube.com/'},
                  title: 'youtube',
                  favicon: 'data/image',
                },
                navigationType: 1,
              },
            ],
          },
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        },
        config);
    return Promise.resolve({success: true});
  }

  override getSession(): Promise<{result: SessionResult}> {
    return Promise.resolve({
      result: {
        config: {
          sessionDuration: {
            microseconds: 120000000n,
          },
          sessionStartTime: {
            msec: 1000000,
          },
          teacher: {
            id: '0',
            name: 'teacher',
            email: 'teacher@gmail.com',
            photoUrl: {url: 'cdn0'},
          },
          accessCode: 'testCode',
          students: [
            {
              id: '1',
              name: 'cat',
              email: 'cat@gmail.com',
              photoUrl: {url: 'cdn1'},
            },
            {
              id: '2',
              name: 'dog',
              email: 'dog@gmail.com',
              photoUrl: {url: 'cdn2'},
            },
          ],
          studentsJoinViaCode: [{
            id: '3',
            name: 'cat1',
            email: 'cat1@gmail.com',
            photoUrl: {url: 'cdn3'},
          }],
          onTaskConfig: {
            isLocked: true,
            tabs: [
              {
                tab: {
                  url: {url: 'http://google.com/'},
                  title: 'google',
                  favicon: 'data/image',
                },
                navigationType: 0,
              },
              {
                tab: {
                  url: {url: 'http://youtube.com/'},
                  title: 'youtube',
                  favicon: 'data/image',
                },
                navigationType: 1,
              },
            ],
          },
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        },
      },
    });
  }

  override updateOnTaskConfig(config: OnTaskConfig):
      Promise<{error: UpdateSessionError | null}> {
    assertDeepEquals(
        {
          isLocked: true,
          tabs: [
            {
              tab: {
                url: {url: 'http://google.com/'},
                title: 'google',
                favicon: 'data/image',
              },
              navigationType: 0,
            },
            {
              tab: {
                url: {url: 'http://youtube.com/'},
                title: 'youtube',
                favicon: 'data/image',
              },
              navigationType: 1,
            },
          ],
        },
        config);
    return Promise.resolve({error: null});
  }

  override updateCaptionConfig(config: CaptionConfig):
      Promise<{error: UpdateSessionError | null}> {
    assertDeepEquals(
        {
          sessionCaptionEnabled: true,
          sessionTranslationEnabled: true,
          localCaptionEnabled: true,
        },
        config);
    return Promise.resolve({error: null});
  }

  override endSession(): Promise<{error: UpdateSessionError | null}> {
    return Promise.resolve({error: null});
  }

  override removeStudent(id: string):
      Promise<{error: RemoveStudentError | null}> {
    id;
    return Promise.resolve({error: null});
  }
  override setFloatMode(isFloatMode: boolean): Promise<{success: boolean}> {
    isFloatMode;
    return Promise.resolve({success: true});
  }
  override submitAccessCode(code: string):
      Promise<{error: SubmitAccessCodeError | null}> {
    if (code === 'valid') {
      return Promise.resolve({error: null});
    } else {
      return Promise.resolve({error: SubmitAccessCodeError.kInvalid});
    }
  }
}

suite('ClientDelegateTest', function() {
  let clientDelegateImpl: ClientDelegateFactory;

  setup(function() {
    clientDelegateImpl = new ClientDelegateFactory(new MockRemoteHandler());
  });

  test(
      'client delegate should properly translate mojom layer data for windows' +
          'list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getWindowsTabsList();

        assertDeepEquals(
            [
              {
                windowName: 'window1',
                tabList: [
                  {title: 'title1', url: 'http://foo1', favicon: 'dataurl1'},
                  {title: 'title2', url: 'http://foo2', favicon: 'dataurl2'},
                ],
              },
              {
                // Default window name should be empty
                windowName: '',
                tabList: [
                  {title: 'title3', url: 'http://foo3', favicon: 'dataurl3'},
                ],
              },
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for course' +
          'list',
      async () => {
        const result = await clientDelegateImpl.getInstance().getCourseList();
        assertDeepEquals(
            [
              {id: '1', name: 'course1', section: ''},
              {id: '2', name: 'course2', section: ''},
            ],
            result);
      });

  test(
      'client delegate should properly translate mojom layer data for student' +
          'list',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().getStudentList('1');

        assertDeepEquals(
            [
              {id: '1', name: 'cat', email: 'email1', photoUrl: 'cdn1'},
              {id: '2', name: 'dog', email: 'email2', photoUrl: 'cdn2'},
            ],
            result);
      });

  test(
      'client delegate should translate data for creating session',
      async () => {
        const result = await clientDelegateImpl.getInstance().createSession({
          sessionDurationInMinutes: 120,
          students: [
            {id: '1', name: 'cat', email: 'cat@gmail.com', photoUrl: 'cdn1'},
            {id: '2', name: 'dog', email: 'dog@gmail.com', photoUrl: 'cdn2'},
          ],
          studentsJoinViaCode: [],
          teacher: undefined,
          accessCode: undefined,
          sessionStartTime: undefined,
          onTaskConfig: {
            isLocked: true,
            tabs: [
              {
                tab: {
                  title: 'google',
                  url: 'http://google.com/',
                  favicon: 'data/image',
                },
                navigationType: 0,
              },
              {
                tab: {
                  title: 'youtube',
                  url: 'http://youtube.com/',
                  favicon: 'data/image',
                },
                navigationType: 1,
              },
            ],
          },
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        });
        assertTrue(result);
      });

  test('client delegate should properly translate get session', async () => {
    const result = await clientDelegateImpl.getInstance().getSession();
    assertDeepEquals(
        {
          sessionConfig: {
            sessionDurationInMinutes: 2,
            sessionStartTime: new Date(1000000),
            teacher: {
              id: '0',
              name: 'teacher',
              email: 'teacher@gmail.com',
              photoUrl: 'cdn0',
            },
            students: [
              {id: '1', name: 'cat', email: 'cat@gmail.com', photoUrl: 'cdn1'},
              {id: '2', name: 'dog', email: 'dog@gmail.com', photoUrl: 'cdn2'},
            ],
            studentsJoinViaCode: [
              {id: '3', name: 'cat1', email: 'cat1@gmail.com', photoUrl: 'cdn3'}
            ],
            accessCode: 'testCode',
            onTaskConfig: {
              isLocked: true,
              tabs: [
                {
                  tab: {
                    title: 'google',
                    url: 'http://google.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 0,
                },
                {
                  tab: {
                    title: 'youtube',
                    url: 'http://youtube.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 1,
                },
              ],
            },
            captionConfig: {
              sessionCaptionEnabled: true,
              localCaptionEnabled: true,
              sessionTranslationEnabled: true,
            },
          },
          activity: [],
        },
        result);
  });

  test(
      'client delegate should properly translate getSession with default value',
      async () => {
        const session = {
          sessionDuration: {
            microseconds: 120000000n,
          },
          sessionStartTime: {
            msec: 1000000,
          },
          students: [],
          studentsJoinViaCode: [],
          onTaskConfig: {isLocked: false, tabs: []},
          teacher: {
            id: '0',
            name: 'teacher',
            email: 'teacher@gmail.com',
            photoUrl: {url: 'cdn0'},
          },
          accessCode: null,
          captionConfig: {
            sessionCaptionEnabled: true,
            localCaptionEnabled: true,
            sessionTranslationEnabled: true,
          },
        };
        const result = getSessionConfigMojomToUI(session);
        assertDeepEquals(
            {
              sessionDurationInMinutes: 2,
              sessionStartTime: new Date(1000000),
              teacher: {
                id: '0',
                name: 'teacher',
                email: 'teacher@gmail.com',
                photoUrl: 'cdn0',
              },
              students: [],
              studentsJoinViaCode: [],
              onTaskConfig: {
                isLocked: false,
                tabs: [],
              },
              accessCode: '',
              captionConfig: {
                sessionCaptionEnabled: true,
                localCaptionEnabled: true,
                sessionTranslationEnabled: true,
              },
            },

            result);
      });

  test(
      'client delegate should translate data for update on task config',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().updateOnTaskConfig({
              isLocked: true,
              tabs: [
                {
                  tab: {
                    title: 'google',
                    url: 'http://google.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 0,
                },
                {
                  tab: {
                    title: 'youtube',
                    url: 'http://youtube.com/',
                    favicon: 'data/image',
                  },
                  navigationType: 1,
                },
              ],
            });
        assertTrue(result);
      });

  test('client delegate should translate data for caption config', async () => {
    const result = await clientDelegateImpl.getInstance().updateCaptionConfig({
      sessionCaptionEnabled: true,
      localCaptionEnabled: true,
      sessionTranslationEnabled: true,
    });
    assertTrue(result);
  });

  test('client delegate should translate data for end session', async () => {
    const result = await clientDelegateImpl.getInstance().endSession();
    assertTrue(result);
  });

  test('client delegate should translate data for remove student', async () => {
    const result = await clientDelegateImpl.getInstance().removeStudent('1');
    assertTrue(result);
  });

  test(
      'client delegate should translate data for student activity',
      async () => {
        const activities = [
          {
            id: '1',
            activity: {
              isActive: true,
              activeTab: 'google',
              isCaptionEnabled: false,
              isHandRaised: false,
              joinMethod: 0
            }
          },
          {
            id: '2',
            activity: {
              isActive: false,
              activeTab: 'youtube',
              isCaptionEnabled: false,
              isHandRaised: false,
              joinMethod: 1
            }
          }
        ];
        const result = getStudentActivityMojomToUI(activities);
        assertDeepEquals(
            [
              {
                id: '1',
                studentActivity: {
                  isActive: true,
                  activeTab: 'google',
                  isCaptionEnabled: false,
                  isHandRaised: false,
                  joinMethod: 0
                }
              },
              {
                id: '2',
                studentActivity: {
                  isActive: false,
                  activeTab: 'youtube',
                  isCaptionEnabled: false,
                  isHandRaised: false,
                  joinMethod: 1
                }
              }
            ],
            result);
      });

  test('client delegate should translate data for set float', async () => {
    const result = await clientDelegateImpl.getInstance().setFloatMode(true);
    assertTrue(result);
  });

  test(
      'client delegate should translate data for submit access code',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().submitAccessCode('valid');
        assertDeepEquals(1, result);
      });

  test(
      'client delegate should return error for invalid access code',
      async () => {
        const result =
            await clientDelegateImpl.getInstance().submitAccessCode('invalid');
        assertDeepEquals(2, result);
      });

  test('client delegate should translate data for network info', async () => {
    const networks = [
      {state: 0, type: 0, name: 'network1', signalStrength: 50},
      {state: 1, type: 1, name: 'network2', signalStrength: 75}
    ];
    const result = getNetworkInfoMojomToUI(networks);
    assertDeepEquals(
        [
          {
            networkState: 0,
            networkType: 0,
            name: 'network1',
            signalStrength: 50
          },
          {
            networkState: 1,
            networkType: 1,
            name: 'network2',
            signalStrength: 75
          }
        ],
        result);
  });
});

from telemetry.page import page as page_module
from telemetry import story

class Cobalt4KVideoPage(page_module.Page):
    def __init__(self, page_set):
        # We use a specific 4K video URL. 
        # Adding 'autoplay=1' and 't=0' to ensure it starts immediately.
        url = 'https://www.youtube.com/tv#/watch?v=TmDKbUrSYxQ&autoplay=1'
        super(Cobalt4KVideoPage, self).__init__(
            url=url,
            page_set=page_set,
            name='4K_Video_Playback_5Min')

    def RunPageInteractions(self, action_runner):
        # 1. Wait for the video element to be present in the DOM
        action_runner.WaitForElement(selector='video')
        
        # 2. Wait for 5 minutes (300 seconds)
        # During this time, Perfetto will be recording your memory/CPU metrics
        action_runner.Wait(300)

class CobaltVideoStorySet(story.StorySet):
    def __init__(self):
        super(CobaltVideoStorySet, self).__init__(
            archive_data_file='data/cobalt_video.json',
            cloud_storage_bucket=story.PARTNER_BUCKET)
        self.AddStory(Cobalt4KVideoPage(self))
(async function(testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <div lang="zh_Hans" id="hundred_chinese_surnames">`
              + '百家姓 '
              + '趙錢孫李 周吳鄭王 馮陳褚衛 蔣沈韓楊 朱秦尤許 何呂施張 孔曹嚴華 金魏陶薑 戚謝鄒喻 柏水竇章 雲蘇潘葛 奚範彭郎 魯韋昌馬 苗鳳花方 俞任袁柳 酆鮑史唐 費廉岑薛 雷賀倪湯 滕殷羅畢 郝鄔安常 '
              + '樂於時傅 皮卞齊康 伍餘元蔔 顧孟平黃 和穆蕭尹 姚邵堪汪 祁毛禹狄 米貝明臧 計伏成戴 談宋茅龐 熊紀舒屈 項祝董梁 杜阮藍閔 席季麻強 賈路婁危 江童顏郭 梅盛林刁 鍾徐邱駱 高夏蔡田 樊胡淩霍 '
              + '虞萬支柯 昝管盧莫 經房裘繆 幹解應宗 丁宣賁鄧 鬱單杭洪 包諸左石 崔吉鈕龔 程嵇邢滑 裴陸榮翁 荀羊於惠 甄曲家封 芮羿儲靳 汲邴糜松 井段富巫 烏焦巴弓 牧隗山穀 車侯宓蓬 全郗班仰 秋仲伊宮 '
              + '寧仇欒暴 甘鈄厲戎 祖武符劉 景詹束龍 葉幸司韶 郜黎薊薄 印宿白懷 蒲台從鄂 索鹹籍賴 卓藺屠蒙 池喬陰鬱 胥能蒼雙 聞莘黨翟 譚貢勞逄 姬申扶堵 冉宰酈雍 卻璩桑桂 濮牛壽通 邊扈燕冀 郟浦尚農 '
              + '溫別莊晏 柴瞿閻充 慕連茹習 宦艾魚容 向古易慎 戈廖庚終 暨居衡步 都耿滿弘 匡國文寇 廣祿闕東 毆殳沃利 蔚越夔隆 師鞏厙聶 晁勾敖融 冷訾辛闞 那簡饒空 曾毋沙乜 養鞠須豐 巢關蒯相 查後荊紅 '
              + '遊竺權逯 蓋益桓公 萬俟司馬 上官歐陽 夏侯諸葛 聞人東方 赫連皇甫 尉遲公羊 澹台公冶 宗政濮陽 淳於單於 太叔申屠 公孫仲孫 軒轅令狐 鐘離宇文 長孫慕容 鮮於閭丘 司徒司空 亓官司寇 仉督子車 '
              + '顓孫端木 巫馬公西 漆雕樂正 壤駟公良 拓拔夾穀 宰父穀粱 晉楚閆法 汝鄢塗欽 段幹百里 東郭南門 呼延歸海 羊舌微生 嶽帥緱亢 況後有琴 梁丘左丘 東門西門 商牟佘佴 伯賞南宮 墨哈譙笪 年愛陽佟'
            + `</div>
            <div lang="ja" id="japanese_iroha">いろはにほへと ちりぬるを わかよたれそ つねならむ うゐのおくやま けふこえて あさき ゆめみし ゑひもせす（ん）色は匂へど 散りぬるを 我が世誰ぞ 常ならむ 有為の奥山 今日越えて 浅き夢見じ 酔ひもせず（ん）</div>
            <div lang="ko" id="korean_pangram">키스의 고유조건은 입술끼리 만나야 하고 특별한 기술은 필요치 않다.</div>
            <div lang="hi" id="hindi_pangram">ऋषियों को सताने वाले दुष्ट राक्षसों के राजा रावण का सर्वनाश करने वाले विष्णुवतार भगवान श्रीराम, अयोध्या के महाराज दशरथ के बड़े सपुत्र थे।</div>
            <div lang="ar" id="arabic_pangram">نصٌّ حكيمٌ لهُ سِرٌّ قاطِعٌ وَذُو شَأنٍ عَظيمٍ مكتوبٌ على ثوبٍ أخضرَ ومُغلفٌ بجلدٍ أزرق</div>
            <div id="emoji">🌱🌲🌳🌴🌵🌷🌸🌹🌺🌻🌼💐🌾🌿🍀🍁🍂🍃🍄🌰☺️😀👪</div>
            <div id="egyptian_hieroglyphs">𓀀𓀁𓀂𓀃𓀄𓀅𓀆𓀇𓀈𓀉𓀊𓀋𓀌𓀍𓀎𓀏</div>
            <div lang="km" id="khmer">ខ្ញុំអាចញុំកញ្ចក់បាន ដោយគ្មានបញ្ហារ</div>
            <div id="gothic">𐌲𐌿𐍄𐌹𐍃𐌺</div>
            <div lang="syr" id="syriac">ܐܬܘܪܝܐ</div>
            <div id="text_presentation_arrows_maths">⇦⇧⇨⇩&#xffe9;&#xffea;&#xffeb;&#xffec;&#x27c0;</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This class acts as the persistent store for all static data
 * about commands.
 *
 * This store can safely be used within either a content or background script
 * context.
 *
 * If you are looking to add a user command, follow the below steps for best
 * integration with existing components:
 * 1. Add the command to the |Command| enum in command.js.
 * 2. Add a command below in CommandStore.COMMAND_DATA. Fill in each of the
 * relevant JSON keys.
 * Be sure to add a msg id and define it in chromevox/messages/messages.js which
 * describes the command. Please also add a category msg id so that the command
 * will show up in the options page.
 * 2. Add the command's logic to CommandHandler inside of our switch-based
 * dispatch method (onCommand).
 * 3. Add a key binding to KeySequence.
 */

import {KeyCode} from '../../common/key_code.js';

import {Command, CommandCategory} from './command.js';
import {KeyBinding, KeySequence, SerializedKeySequence} from './key_sequence.js';

export class CommandStore {
  /**
   * Gets a message given a command.
   * @param {!Command} command The command to query.
   * @return {string|undefined} The message id, if any.
   */
  static messageForCommand(command) {
    return CommandStore.COMMAND_DATA[command]?.msgId;
  }

  /**
   * Gets a category given a command.
   * @param {!Command} command The command to query.
   * @return {!CommandCategory|undefined} The category, if any.
   */
  static categoryForCommand(command) {
    return CommandStore.COMMAND_DATA[command]?.category;
  }

  /**
   * Gets the first command associated with the message id
   * @param {string} msgId
   * @return {!Command|undefined} The command, if any.
   */
  static commandForMessage(msgId) {
    for (const commandName in CommandStore.COMMAND_DATA) {
      const command = CommandStore.COMMAND_DATA[commandName];
      if (command.msgId === msgId) {
        return commandName;
      }
    }
  }

  /**
   * Gets all commands for a category.
   * @param {!CommandCategory} category The category to query.
   * @return {Array<!Command>} The commands, if any.
   */
  static commandsForCategory(category) {
    const ret = [];
    for (const cmd in CommandStore.COMMAND_DATA) {
      const struct = CommandStore.COMMAND_DATA[cmd];
      if (category === struct.category) {
        ret.push(cmd);
      }
    }
    return ret;
  }

  /**
   * @param {!Command} command The command to query.
   * @return {boolean} Whether or not this command is denied in the OOBE.
   */
  static denySignedOut(command) {
    if (!CommandStore.COMMAND_DATA[command]) {
      return false;
    }
    return Boolean(CommandStore.COMMAND_DATA[command].denySignedOut);
  }

  /** @return {!Array<!KeyBinding>} */
  static getKeyBindings() {
    const keyBindings = [];

    // Validate the type of the keyBindings array.
    for (const binding of KEY_BINDINGS_) {
      if (binding.command === undefined || binding.sequence === undefined) {
        throw new Error('Invalid key map.');
      }
      keyBindings.push({
        command: binding.command,
        sequence: KeySequence.deserialize(binding.sequence),
      });
    }
    return keyBindings;
  }
}

/**
 * @typedef {{
 *     category: !CommandCategory,
 *     msgId: (undefined|string),
 *     denySignedOut: (undefined|boolean)
 * }}
 *  category: The command's category.
 *  msgId: The message resource describing the command.
 *  denySignedOut: Explicitly denies this command when on chrome://oobe/* or
 *             other signed-out contexts. Defaults to false.
 */
let DataEntry;

/**
 * Collection of command properties.
 * @type {Object<!Command, !DataEntry>}
 */
CommandStore.COMMAND_DATA = {
  [Command.ANNOUNCE_BATTERY_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_battery_description',
  },
  [Command.ANNOUNCE_HEADERS]: {
    category: CommandCategory.TABLES,
    msgId: 'announce_headers',
  },
  [Command.ANNOUNCE_RICH_TEXT_DESCRIPTION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'announce_rich_text_description',
  },
  [Command.AUTORUNNER]: {category: CommandCategory.NO_CATEGORY},
  [Command.BACKWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'backward',
  },
  [Command.BOTTOM]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_bottom',
  },
  [Command.CONTEXT_MENU]: {
    category: CommandCategory.INFORMATION,
    msgId: 'show_context_menu',
  },
  [Command.CYCLE_PUNCTUATION_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_punctuation_echo',
  },
  [Command.CYCLE_TYPING_ECHO]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'cycle_typing_echo',
  },
  [Command.DEBUG]: {category: CommandCategory.NO_CATEGORY},
  [Command.DECREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_pitch',
  },
  [Command.DECREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_rate',
  },
  [Command.DECREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'decrease_tts_volume',
  },
  [Command.ENABLE_CONSOLE_TTS]: {
    category: CommandCategory.DEVELOPER,
    msgId: 'enable_tts_log',
  },
  [Command.ENTER_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'enter_content',
  },
  [Command.EXIT_SHIFTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'exit_content',
  },
  [Command.EXIT_SHIFTER_CONTENT]: {category: CommandCategory.NO_CATEGORY},
  [Command.FORCE_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.ACTIONS,
    msgId: 'force_click_on_current_item',
  },
  [Command.FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM]:
      {category: CommandCategory.NO_CATEGORY},
  [Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'force_long_click_on_current_item',
  },
  [Command.FORWARD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'forward',
  },
  [Command.FULLY_DESCRIBE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'fully_describe',
  },
  [Command.GO_TO_COL_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_beginning',
  },
  [Command.GO_TO_COL_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_col_end',
  },
  [Command.GO_TO_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_beginning',
  },
  [Command.GO_TO_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_end',
  },
  [Command.GO_TO_ROW_FIRST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_beginning',
  },
  [Command.GO_TO_ROW_LAST_CELL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_row_end',
  },
  [Command.HANDLE_TAB]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_next',
  },
  [Command.HANDLE_TAB_PREV]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'handle_tab_prev',
  },
  [Command.HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'help',
  },
  [Command.INCREASE_TTS_PITCH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_pitch',
  },
  [Command.INCREASE_TTS_RATE]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_rate',
  },
  [Command.INCREASE_TTS_VOLUME]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'increase_tts_volume',
  },
  [Command.JUMP_TO_BOTTOM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_bottom',
  },
  [Command.JUMP_TO_DETAILS]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_details',
  },
  [Command.JUMP_TO_TOP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'jump_to_top',
  },
  [Command.LEFT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'left',
  },
  [Command.LINE_DOWN]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_down',
  },
  [Command.LINE_UP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_line_up',
  },
  [Command.MOVE_TO_START_OF_LINE]: {category: CommandCategory.NO_CATEGORY},
  [Command.MOVE_TO_END_OF_LINE]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_ARTICLE]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_at_granularity',
  },
  [Command.NEXT_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_button',
  },
  [Command.NEXT_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_character',
  },
  [Command.NEXT_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_checkbox',
  },
  [Command.NEXT_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_col',
  },
  [Command.NEXT_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_combo_box',
  },
  [Command.NEXT_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_edit_text',
  },
  [Command.NEXT_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_form_field',
  },
  [Command.NEXT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_granularity',
  },
  [Command.NEXT_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_graphic',
  },
  [Command.NEXT_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_group',
  },
  [Command.NEXT_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading',
  },
  [Command.NEXT_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading1',
  },
  [Command.NEXT_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading2',
  },
  [Command.NEXT_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading3',
  },
  [Command.NEXT_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading4',
  },
  [Command.NEXT_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading5',
  },
  [Command.NEXT_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_heading6',
  },
  [Command.NEXT_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_invalid_item',
  },
  [Command.NEXT_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_landmark',
  },
  [Command.NEXT_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_line',
  },
  [Command.NEXT_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_link',
  },
  [Command.NEXT_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_list',
  },
  [Command.NEXT_LIST_ITEM]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_list_item',
  },
  [Command.NEXT_MATH]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_math',
  },
  [Command.NEXT_MEDIA]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_media',
  },
  [Command.NEXT_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_object',
  },
  [Command.NEXT_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_radio',
  },
  [Command.NEXT_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_next_row',
  },
  [Command.NEXT_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_sentence',
  },
  [Command.NEXT_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_similar_item',
  },
  [Command.NEXT_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.NEXT_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_table',
  },
  [Command.NEXT_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'next_visited_link',
  },
  [Command.NEXT_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'next_word',
  },
  [Command.NOP]: {category: CommandCategory.NO_CATEGORY},
  [Command.OPEN_CHROMEVOX_MENUS]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'menus_title',
  },
  [Command.OPEN_LONG_DESC]: {
    category: CommandCategory.INFORMATION,
    msgId: 'open_long_desc',
  },
  [Command.PAN_LEFT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_left',
  },
  [Command.PAN_RIGHT]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_pan_right',
  },
  [Command.PASS_THROUGH_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'pass_through_key_description',
  },
  [Command.PAUSE_ALL_MEDIA]: {
    category: CommandCategory.INFORMATION,
    msgId: 'pause_all_media',
  },
  [Command.PREVIOUS_ARTICLE]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_AT_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_at_granularity',
  },
  [Command.PREVIOUS_BUTTON]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_button',
  },
  [Command.PREVIOUS_CHARACTER]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_character',
  },
  [Command.PREVIOUS_CHECKBOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_checkbox',
  },
  [Command.PREVIOUS_COL]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_col',
  },
  [Command.PREVIOUS_COMBO_BOX]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_combo_box',
  },
  [Command.PREVIOUS_CONTROL]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_EDIT_TEXT]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_edit_text',
  },
  [Command.PREVIOUS_FORM_FIELD]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_form_field',
  },
  [Command.PREVIOUS_GRANULARITY]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_granularity',
  },
  [Command.PREVIOUS_GRAPHIC]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_graphic',
  },
  [Command.PREVIOUS_GROUP]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_group',
  },
  [Command.PREVIOUS_HEADING]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading',
  },
  [Command.PREVIOUS_HEADING_1]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading1',
  },
  [Command.PREVIOUS_HEADING_2]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading2',
  },
  [Command.PREVIOUS_HEADING_3]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading3',
  },
  [Command.PREVIOUS_HEADING_4]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading4',
  },
  [Command.PREVIOUS_HEADING_5]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading5',
  },
  [Command.PREVIOUS_HEADING_6]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_heading6',
  },
  [Command.PREVIOUS_INVALID_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_invalid_item',
  },
  [Command.PREVIOUS_LANDMARK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_landmark',
  },
  [Command.PREVIOUS_LINE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_line',
  },
  [Command.PREVIOUS_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_link',
  },
  [Command.PREVIOUS_LIST]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_list',
  },
  [Command.PREVIOUS_LIST_ITEM]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_list_item',
  },
  [Command.PREVIOUS_MATH]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_math',
  },
  [Command.PREVIOUS_MEDIA]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_media',
  },
  [Command.PREVIOUS_OBJECT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_object',
  },
  [Command.PREVIOUS_RADIO]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_radio',
  },
  [Command.PREVIOUS_ROW]: {
    category: CommandCategory.TABLES,
    msgId: 'skip_to_prev_row',
  },
  [Command.PREVIOUS_SECTION]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_SENTENCE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_sentence',
  },
  [Command.PREVIOUS_SIMILAR_ITEM]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_similar_item',
  },
  [Command.PREVIOUS_SLIDER]: {category: CommandCategory.NO_CATEGORY},
  [Command.PREVIOUS_TABLE]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_table',
  },
  [Command.PREVIOUS_VISITED_LINK]: {
    category: CommandCategory.JUMP_COMMANDS,
    msgId: 'previous_visited_link',
  },
  [Command.PREVIOUS_WORD]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'previous_word',
  },
  [Command.READ_CURRENT_TITLE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_title',
  },
  [Command.READ_CURRENT_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_current_url',
  },
  [Command.READ_FROM_HERE]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'read_from_here',
  },
  [Command.READ_LINK_URL]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_link_url',
  },
  [Command.READ_PHONETIC_PRONUNCIATION]: {
    category: CommandCategory.INFORMATION,
    msgId: 'read_phonetic_pronunciation',
  },
  [Command.REPORT_ISSUE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'panel_menu_item_report_issue',
  },
  [Command.RESET_TEXT_TO_SPEECH_SETTINGS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'reset_tts_settings',
  },
  [Command.RIGHT]: {
    category: CommandCategory.NAVIGATION,
    msgId: 'right',
  },
  [Command.ROUTING]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_routing',
  },
  [Command.SCROLL_BACKWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_backward_description',
  },
  [Command.SCROLL_FORWARD]: {
    category: CommandCategory.NO_CATEGORY,
    msgId: 'action_scroll_forward_description',
  },
  [Command.SHOW_FORMS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_forms_list',
  },
  [Command.SHOW_HEADINGS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_headings_list',
  },
  [Command.SHOW_LANDMARKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_landmarks_list',
  },
  [Command.SHOW_LEARN_MODE_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_kb_explorer_page',
  },
  [Command.SHOW_LINKS_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_links_list',
  },
  [Command.SHOW_LOG_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_log_page',
  },
  [Command.SHOW_OPTIONS_PAGE]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_options_page',
  },
  [Command.SHOW_PANEL_MENU_MOST_RECENT]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
  },
  [Command.SHOW_TABLES_LIST]: {
    category: CommandCategory.OVERVIEW,
    msgId: 'show_tables_list',
  },
  [Command.SHOW_TTS_SETTINGS]: {
    category: CommandCategory.HELP_COMMANDS,
    denySignedOut: true,
    msgId: 'show_tts_settings',
  },
  [Command.SPEAK_TIME_AND_DATE]: {
    category: CommandCategory.INFORMATION,
    msgId: 'speak_time_and_date',
  },
  [Command.SPEAK_TABLE_LOCATION]: {
    category: CommandCategory.TABLES,
    msgId: 'speak_table_location',
  },
  [Command.START_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.STOP_HISTORY_RECORDING]: {category: CommandCategory.NO_CATEGORY},
  [Command.STOP_SPEECH]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'stop_speech_key',
  },
  [Command.TOGGLE_BRAILLE_CAPTIONS]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'braille_captions',
  },
  [Command.TOGGLE_BRAILLE_TABLE]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'toggle_braille_table',
  },
  [Command.TOGGLE_DICTATION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_dictation',
  },
  [Command.TOGGLE_EARCONS]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'toggle_earcons',
  },
  [Command.TOGGLE_KEYBOARD_HELP]: {
    category: CommandCategory.HELP_COMMANDS,
    msgId: 'show_panel_menu',
  },
  [Command.TOGGLE_SCREEN]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_screen',
  },
  [Command.TOGGLE_SEARCH_WIDGET]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_search_widget',
  },
  [Command.TOGGLE_SELECTION]: {
    category: CommandCategory.ACTIONS,
    msgId: 'toggle_selection',
  },
  [Command.TOGGLE_SEMANTICS]: {
    category: CommandCategory.INFORMATION,
    msgId: 'toggle_semantics',
  },
  [Command.TOGGLE_SPEECH_ON_OR_OFF]: {
    category: CommandCategory.CONTROLLING_SPEECH,
    msgId: 'speech_on_off_description',
  },
  [Command.TOGGLE_STICKY_MODE]: {
    category: CommandCategory.MODIFIER_KEYS,
    msgId: 'toggle_sticky_mode',
  },
  [Command.TOP]: {
    category: CommandCategory.BRAILLE,
    msgId: 'braille_top',
  },
  [Command.VIEW_GRAPHIC_AS_BRAILLE]: {
    category: CommandCategory.BRAILLE,
    msgId: 'view_graphic_as_braille',
  },
};

/** @private {!Array<{command: !Command, sequence: !SerializedKeySequence}>} */
const KEY_BINDINGS_ = [
  {
    command: Command.PREVIOUS_OBJECT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT]}},
  },
  {
    command: Command.PREVIOUS_LINE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.UP]}},
  },
  {
    command: Command.NEXT_OBJECT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  {
    command: Command.NEXT_LINE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN]}},
  },
  {
    command: Command.NEXT_CHARACTER,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_CHARACTER,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], shiftKey: [true]}},
  },
  {
    command: Command.NATIVE_NEXT_CHARACTER,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.RIGHT]}},
  },
  {
    command: Command.NATIVE_PREVIOUS_CHARACTER,
    sequence: {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT]}},
  },
  {
    command: Command.NEXT_WORD,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_WORD,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.NATIVE_NEXT_WORD,
    sequence: {
      cvoxModifier: false,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]},
    },
  },
  {
    command: Command.NATIVE_PREVIOUS_WORD,
    sequence:
        {cvoxModifier: false, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
  {
    command: Command.NEXT_BUTTON,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.B]}},
  },
  {
    command: Command.PREVIOUS_BUTTON,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.B], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_CHECKBOX,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.X]}},
  },
  {
    command: Command.PREVIOUS_CHECKBOX,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.X], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_COMBO_BOX,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.C]}},
  },
  {
    command: Command.PREVIOUS_COMBO_BOX,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_EDIT_TEXT,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.E]}},
  },
  {
    command: Command.PREVIOUS_EDIT_TEXT,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.E], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_FORM_FIELD,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.F]}},
  },
  {
    command: Command.PREVIOUS_FORM_FIELD,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_GRAPHIC,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_GRAPHIC,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.G]}},
  },
  {
    command: Command.NEXT_HEADING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.H]}},
  },
  {
    command: Command.NEXT_HEADING_1,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE]}},
  },
  {
    command: Command.NEXT_HEADING_2,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO]}},
  },
  {
    command: Command.NEXT_HEADING_3,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.THREE]}},
  },
  {
    command: Command.NEXT_HEADING_4,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR]}},
  },
  {
    command: Command.NEXT_HEADING_5,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE]}},
  },
  {
    command: Command.NEXT_HEADING_6,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX]}},
  },
  {
    command: Command.PREVIOUS_HEADING,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_1,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.ONE], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_2,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.TWO], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_3,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.THREE], shiftKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_HEADING_4,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FOUR], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_5,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.FIVE], shiftKey: [true]}},
  },
  {
    command: Command.PREVIOUS_HEADING_6,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SIX], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_LINK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.L]}},
  },
  {
    command: Command.PREVIOUS_LINK,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_TABLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.T]}},
  },
  {
    command: Command.PREVIOUS_TABLE,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_VISITED_LINK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.V]}},
  },
  {
    command: Command.PREVIOUS_VISITED_LINK,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.V], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_LANDMARK,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1]}},
  },
  {
    command: Command.PREVIOUS_LANDMARK,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_1], shiftKey: [true]},
    },
  },
  {
    command: Command.JUMP_TO_BOTTOM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true]}},
  },
  {
    command: Command.JUMP_TO_TOP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true]}},
  },
  {
    command: Command.FORCE_CLICK_ON_CURRENT_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}},
  },
  {
    command: Command.FORCE_LONG_CLICK_ON_CURRENT_ITEM,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.SPACE], shiftKey: [true]},
    },
  },
  {
    command: Command.CONTEXT_MENU,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.M]}},
  },
  {
    command: Command.READ_FROM_HERE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.R]}},
  },
  {
    command: Command.TOGGLE_STICKY_MODE,
    sequence: {
      skipStripping: false,
      doubleTap: true,
      keys: {keyCode: [KeyCode.SEARCH]},
    },
  },
  {
    command: Command.PASS_THROUGH_MODE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.ESCAPE], shiftKey: [true]},
    },
  },
  {
    command: Command.TOGGLE_KEYBOARD_HELP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_PERIOD]}},
  },
  {
    command: Command.STOP_SPEECH,
    sequence: {
      cvoxModifier: false,
      keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]},
    },
  },
  {
    command: Command.DECREASE_TTS_RATE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_4], shiftKey: [true]},
    },
  },
  {
    command: Command.INCREASE_TTS_RATE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_4]}},
  },
  {
    command: Command.DECREASE_TTS_PITCH,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_6], shiftKey: [true]},
    },
  },
  {
    command: Command.INCREASE_TTS_PITCH,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_6]}},
  },
  {
    command: Command.STOP_SPEECH,
    sequence: {keys: {ctrlKey: [true], keyCode: [KeyCode.CONTROL]}},
  },
  {
    command: Command.CYCLE_PUNCTUATION_ECHO,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.P]}},
  },
  {
    command: Command.SHOW_LEARN_MODE_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.K]}},
  },
  {
    command: Command.CYCLE_TYPING_ECHO,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.T]}},
  },
  {
    command: Command.SHOW_OPTIONS_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.O]}},
  },
  {
    command: Command.SHOW_LOG_PAGE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.W]}},
  },
  {
    command: Command.ENABLE_LOGGING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.E]}},
  },
  {
    command: Command.DISABLE_LOGGING,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.D]}},
  },
  {
    command: Command.DUMP_TREE,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.D, KeyCode.T], ctrlKey: [true]},
    },
  },
  {
    command: Command.HELP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.T]}},
  },
  {
    command: Command.TOGGLE_EARCONS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.E]}},
  },
  {
    command: Command.SPEAK_TIME_AND_DATE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.D]}},
  },
  {
    command: Command.READ_CURRENT_TITLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.W]}},
  },
  {
    command: Command.READ_CURRENT_URL,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.U]}},
  },
  {
    command: Command.REPORT_ISSUE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.I]}},
  },
  {
    command: Command.TOGGLE_SEARCH_WIDGET,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_2]}},
  },
  {
    command: Command.SHOW_HEADINGS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.H], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_FORMS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.F], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_LANDMARKS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.OEM_1], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_LINKS_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.L], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_ACTIONS_MENU,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.A], ctrlKey: [true]}},
  },
  {
    command: Command.SHOW_TABLES_LIST,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.T], ctrlKey: [true]}},
  },
  {
    command: Command.TOGGLE_BRAILLE_CAPTIONS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.B]}},
  },
  {
    command: Command.TOGGLE_BRAILLE_TABLE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.G]}},
  },
  {
    command: Command.VIEW_GRAPHIC_AS_BRAILLE,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.G], altKey: [true]}},
  },
  {
    command: Command.TOGGLE_SELECTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.S]}},
  },
  {
    command: Command.FULLY_DESCRIBE,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.K]}},
  },
  {
    command: Command.PREVIOUS_ROW,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.UP], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.NEXT_ROW,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.NEXT_COL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.PREVIOUS_COL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], ctrlKey: [true], altKey: [true]},
    },
  },
  {
    command: Command.GO_TO_ROW_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.LEFT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_COL_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.UP],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_COL_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.DOWN],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.GO_TO_FIRST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.LEFT], altKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.GO_TO_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.RIGHT], altKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.GO_TO_ROW_LAST_CELL,
    sequence: {
      cvoxModifier: true,
      keys: {
        keyCode: [KeyCode.RIGHT],
        ctrlKey: [true],
        altKey: [true],
        shiftKey: [true],
      },
    },
  },
  {
    command: Command.PREVIOUS_GROUP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.UP], ctrlKey: [true]}},
  },
  {
    command: Command.NEXT_GROUP,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.DOWN], ctrlKey: [true]}},
  },
  {
    command: Command.PREVIOUS_SIMILAR_ITEM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.I], shiftKey: [true]}},
  },
  {
    command: Command.NEXT_SIMILAR_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.I]}},
  },
  {
    command: Command.PREVIOUS_INVALID_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.P, KeyCode.I]}},
  },
  {
    command: Command.NEXT_INVALID_ITEM,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.N, KeyCode.I]}},
  },
  {
    command: Command.JUMP_TO_DETAILS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.J]}},
  },
  {
    command: Command.TOGGLE_SCREEN,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.BRIGHTNESS_UP]}},
  },
  {
    command: Command.TOGGLE_SPEECH_ON_OR_OFF,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.VOLUME_MUTE]}},
  },
  {
    command: Command.ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_4]}},
  },
  {
    command: Command.DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.OEM_6]}},
  },
  {
    command: Command.SHOW_TALKBACK_KEYBOARD_SHORTCUTS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.K]}},
  },
  {
    command: Command.FORCE_CLICK_ON_CURRENT_ITEM,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.SPACE]}, doubleTap: true},
  },
  {
    command: Command.SHOW_TTS_SETTINGS,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.S]}},
  },
  {
    command: Command.ANNOUNCE_BATTERY_DESCRIPTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.O, KeyCode.B]}},
  },
  {
    command: Command.ANNOUNCE_RICH_TEXT_DESCRIPTION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.F]}},
  },
  {
    command: Command.READ_PHONETIC_PRONUNCIATION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.C]}},
  },
  {
    command: Command.READ_LINK_URL,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.A, KeyCode.L]}},
  },
  {
    command: Command.NEXT_LIST,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.J, KeyCode.L]}},
  },
  {
    command: Command.PREVIOUS_LIST,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.J, KeyCode.L], shiftKey: [true]},
    },
  },
  {
    command: Command.RESET_TEXT_TO_SPEECH_SETTINGS,
    sequence: {
      cvoxModifier: true,
      keys: {keyCode: [KeyCode.OEM_5], ctrlKey: [true], shiftKey: [true]},
    },
  },
  {
    command: Command.COPY,
    sequence:
        {cvoxModifier: true, keys: {keyCode: [KeyCode.C], ctrlKey: [true]}},
  },
  {
    command: Command.TOGGLE_DICTATION,
    sequence: {cvoxModifier: true, keys: {keyCode: [KeyCode.D]}},
  },
];

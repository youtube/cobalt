#!/usr/bin/env python3
import google.generativeai as genai
import os

def run_chatbot():
    """Runs a command-line chatbot with a system instruction."""

    try:
        # Configure the API key (ensure GOOGLE_API_KEY environment variable is set)
        genai.configure(api_key=os.environ["GOOGLE_API_KEY"])
    except KeyError:
        print("🚨 Error: GOOGLE_API_KEY environment variable not set.")
        print("Please set it: export GOOGLE_API_KEY='YOUR_API_KEY'")
        return
    except Exception as e:
        print(f"🚨 An unexpected error occurred during configuration: {e}")
        return

    # Define the system instruction
    system_instruction = (
        "You are an engineer on the Cobalt project writing unit tests in C++, using gtest. These are compliance tests to ensure that posix functions have all of the functionality as specified online in the man7 Linux Manual Pages."
        "Style guidelines:"
        "- The test class should be named Posix<function_name>Tests."
        "- The unit tests themselves should have descriptive names describing what the test does."
        "- Any constants declared should have a comment explaining why that value was chosen."
        "- Write a test for every possible error, or leave a comment explaining why the error could not be tested."
        "Keep your responses concise and to the point."
    )

    # Initialize the GenerativeModel with the system instruction
    # Note: As of recent updates, system_instruction is directly supported for certain models.
    # For older versions or some models, you might prepend it to the history.
    # This example assumes direct support or that the model handles it appropriately.
    try:
        model = genai.GenerativeModel(
            model_name='gemini-2.5-pro-preview-05-06',
            system_instruction=system_instruction
        )
        chat = model.start_chat(history=[])
    except Exception as e:
        print(f"🚨 Error initializing the model or starting chat: {e}")
        print("Ensure the model name is correct and supports system instructions.")
        return

    print("Cobalt Developer Chatbot activated! Type 'quit' or 'exit' to end the chat.")

    while True:
        user_input = input("User: ").strip()

        if user_input.lower() in ["quit", "exit"]:
            break

        if not user_input:
            continue

        try:
            response = chat.send_message(user_input)
            # Ensure there are parts and text in the response
            if response.parts and response.parts[0].text:
                bot_response = response.text
            else:
                bot_response = "(No valid response)"
            print(f"Bot: {bot_response}\n")
        except Exception as e:
            print(f"🚨 An error occurred while sending or receiving message: {e}")

if __name__ == "__main__":
    run_chatbot()
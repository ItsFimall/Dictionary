# Flipper Zero English Dictionary App

This is an offline English dictionary application designed for the Flipper Zero.

[中文版 (Chinese Version)](https://github.com/Lynn3t/Dictionary/README.md)

## 1. Features

- **Word Search**: Quickly look up definitions and phonetics for English words.
- **Search History**: Save and review the last 10 queried words for easy review.
- **About Page**: View information about the application's author and data sources.

## 2. How to Build and Install

This application is built using the Flipper Zero SDK tool (ufbt) and is based on the **mntm-009** firmware, which requires manually specifying the SDK version.

A **Python3** environment is required for the build and installation process. Please ensure it is installed on your system.

#### Steps

1. **Clone this repository**

   ```
   git clone https://github.com/Lynn3t/Dictionary.git
   cd Dictionary
   ```

2. **Install ufbt**

   ```
   pip install ufbt
   ```

3. **Install the specified mntm-009 SDK**

   ```
   ufbt update -t f7 -u https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-009/flipper-z-f7-sdk-mntm-009.zip
   ```

4. **Compile the project**

   ```
   ufbt
   ```

5. **Upload to Flipper Zero** After compilation, you can upload the `.fap` file to your device using [qFlipper](https://flipperzero.one/update) or the following `ufbt` command:

   ```
   ufbt launch
   ```

## 3. Credits

The dictionary data for this application is sourced from the following excellent projects. Special thanks to:

- [**google-10000-english**](https://github.com/first20hours/google-10000-english): A list of the 10,000 most common English words, compiled by Google from web corpora.
- [**WordNet**](https://wordnet.princeton.edu/): A large lexical database of English created by Princeton University. (Adheres to the WordNet License)
- [**The CMU Pronouncing Dictionary (CMUdict)**](http://www.speech.cs.cmu.edu/cgi-bin/cmudict): An open-source pronouncing dictionary containing over 134,000 words and their pronunciations.

## 4. License

This application is licensed under the [MIT License](https://www.google.com/search?q=LICENSE). Please see the `LICENSE` file for more details.
FM-1 Firmware Audio Samples
===========================

This directory is reserved for short preview audio clips (20–30 seconds) in MP3 format.

Naming Convention:
------------------
Name audio files using lowercase alphanumeric strings matching each firmware's name or slug:
  audio/<name>.mp3

Examples:
  audio/ghoulbox.mp3
  audio/choralroot.mp3
  audio/dinghy.mp3
  audio/felucca.mp3
  audio/hortator.mp3
  audio/sloop.mp3
  audio/zp12.mp3

Updating firmwares.json:
------------------------
When a sound sample is added, update the corresponding firmware entry in firmwares.json:
  "audio": "audio/<name>.mp3",
  "audio_credit": "<Author Name or Contributor Credit>"

Important Guidelines:
---------------------
Never download, scrape, or rehost anyone's audio, images, or video without explicit permission from the author. Firmwares without an audio clip will display a quiet "Sound sample coming soon" indicator.

{
  "targets": [
    {
      "target_name": "shm-buffer",
      "sources": [
        "src/shm-buffer.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
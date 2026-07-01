import sys
import re
from youtube_transcript_api import YouTubeTranscriptApi

def extract_video_id(url):
    # Strip whitespace
    url = url.strip()
    # Check if already a direct 11-character video ID
    if re.match(r'^[a-zA-Z0-9_-]{11}$', url):
        return url
    
    # Try various URL patterns
    patterns = [
        r'(?:v=|\/v\/|embed\/|shorts\/|youtu\.be\/|watch\?.*?v=)([a-zA-Z0-9_-]{11})'
    ]
    for pattern in patterns:
        match = re.search(pattern, url)
        if match:
            return match.group(1)
    return None

def format_time(seconds):
    s = int(seconds)
    m = s // 60
    s = s % 60
    h = m // 60
    m = m % 60
    if h > 0:
        return f"{h}:{m:02d}:{s:02d}"
    else:
        return f"{m}:{s:02d}"

def main():
    # Force stdout to write UTF-8 to prevent charmap errors on Windows
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')

    if len(sys.argv) < 2:
        print("ERROR: No URL or Video ID provided.", file=sys.stderr)
        sys.exit(1)
        
    input_str = sys.argv[1]
    video_id = extract_video_id(input_str)
    if not video_id:
        print("ERROR: Invalid YouTube URL or Video ID.", file=sys.stderr)
        sys.exit(1)
        
    try:
        # Instantiate the API
        api = YouTubeTranscriptApi()
        transcript_list = api.list(video_id)
        
        # Priority: Manual English/German/Spanish/French, then Auto English/German, then first available
        try:
            transcript = transcript_list.find_manually_created_transcript(['en', 'de', 'es', 'fr', 'ja'])
        except Exception:
            try:
                transcript = transcript_list.find_generated_transcript(['en', 'de', 'es', 'fr', 'ja'])
            except Exception:
                transcript = next(iter(transcript_list))
                
        data = transcript.fetch()
        
        for entry in data:
            # Clean up newlines inside text to prevent breaking C++ line-by-line parser
            text = entry.text.replace('\n', ' ').replace('\r', ' ')
            time_str = format_time(entry.start)
            print(f"{time_str} {text}")
            
    except Exception as e:
        print(f"ERROR: {str(e)}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

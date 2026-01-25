// Test Fish Audio API with key from .env
import 'dotenv/config';

const API_KEY = process.env.VITE_FISH_AUDIO_API_KEY;

console.log('=== Fish Audio API Test ===\n');
console.log('API Key:', API_KEY ? `${API_KEY.substring(0, 8)}...` : 'NOT FOUND');

if (!API_KEY) {
  console.error('❌ No API key found in .env file');
  process.exit(1);
}

async function testFishAudio() {
  try {
    console.log('\n📡 Testing API connection...');
    
    const response = await fetch('https://api.fish.audio/model?page_size=5', {
      headers: {
        'Authorization': `Bearer ${API_KEY}`,
        'Content-Type': 'application/json'
      }
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    const data = await response.json();
    
    console.log('✅ API connection successful!\n');
    console.log(`Total voices available: ${data.total || 0}`);
    console.log(`Voices returned: ${data.items?.length || 0}\n`);
    
    if (data.items && data.items.length > 0) {
      console.log('Sample voices:');
      data.items.slice(0, 3).forEach((voice, i) => {
        console.log(`  ${i + 1}. ${voice.title} (ID: ${voice.id})`);
      });
    }
    
    console.log('\n✅ Fish Audio API is working correctly!');
    
  } catch (error) {
    console.error('\n❌ API Test Failed:');
    console.error('Error:', error.message);
    
    if (error.message.includes('401')) {
      console.error('\n💡 Tip: API key is invalid or expired');
    } else if (error.message.includes('403')) {
      console.error('\n💡 Tip: API key does not have required permissions');
    } else if (error.message.includes('fetch')) {
      console.error('\n💡 Tip: Check your internet connection');
    }
    
    process.exit(1);
  }
}

testFishAudio();

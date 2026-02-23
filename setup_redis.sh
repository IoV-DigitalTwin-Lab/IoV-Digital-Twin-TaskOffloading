#!/bin/bash
# Redis Digital Twin Setup Script

echo "=================================================="
echo "Redis Digital Twin Installation"
echo "=================================================="

# Install Redis server
echo ""
echo "1. Installing Redis server..."
if command -v apt-get &> /dev/null; then
    apt-get update
    apt-get install -y redis-server
elif command -v yum &> /dev/null; then
    yum install -y redis
elif command -v brew &> /dev/null; then
    brew install redis
else
    echo "Error: Package manager not found. Please install Redis manually."
    exit 1
fi

# Install hiredis (Redis C client library)
echo ""
echo "2. Installing hiredis library..."
if command -v apt-get &> /dev/null; then
    apt-get install -y libhiredis-dev
elif command -v yum &> /dev/null; then
    yum install -y hiredis-devel
elif command -v brew &> /dev/null; then
    brew install hiredis
else
    echo "Error: Package manager not found. Please install hiredis manually."
    exit 1
fi

# Start Redis service
echo ""
echo "3. Starting Redis service..."
if command -v systemctl &> /dev/null; then
    systemctl start redis
    systemctl enable redis
    echo "Redis service started and enabled on boot"
elif command -v brew &> /dev/null; then
    brew services start redis
    echo "Redis service started via Homebrew"
else
    echo "Starting Redis manually..."
    redis-server --daemonize yes
fi

# Test Redis connection
echo ""
echo "4. Testing Redis connection..."
if redis-cli ping > /dev/null 2>&1; then
    echo "✓ Redis is running and responding to ping"
    redis-cli INFO server | grep redis_version
else
    echo "✗ Redis connection failed"
    echo "Please check if Redis is running: redis-cli ping"
    exit 1
fi

# Configure Redis for optimal performance
echo ""
echo "5. Configuring Redis..."
redis-cli CONFIG SET maxmemory-policy allkeys-lru
redis-cli CONFIG SET save ""  # Disable RDB snapshots for faster performance
echo "✓ Redis configured with LRU eviction and disabled persistence"

# Test basic operations
echo ""
echo "6. Testing basic Redis operations..."
redis-cli SET test:key "test_value" > /dev/null
TEST_VAL=$(redis-cli GET test:key)
if [ "$TEST_VAL" = "test_value" ]; then
    echo "✓ Redis read/write operations working"
    redis-cli DEL test:key > /dev/null
else
    echo "✗ Redis operations test failed"
fi

echo ""
echo "=================================================="
echo "Redis Digital Twin Setup Complete!"
echo "=================================================="
echo ""
echo "Redis is running on: 127.0.0.1:6379"
echo ""
echo "Useful commands:"
echo "  - Check status: redis-cli ping"
echo "  - Monitor activity: redis-cli monitor"
echo "  - View all keys: redis-cli KEYS '*'"
echo "  - Flush database: redis-cli FLUSHALL"
echo "  - Stop Redis: systemctl stop redis (or brew services stop redis)"
echo ""
echo "Next steps:"
echo "  1. Rebuild the simulation: make clean && make"
echo "  2. Run the simulation with Redis enabled"
echo "  3. Monitor Redis: redis-cli monitor"
echo ""

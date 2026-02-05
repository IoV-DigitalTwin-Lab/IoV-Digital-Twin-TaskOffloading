#!/bin/bash
# Test Redis Digital Twin Integration

echo "=================================================="
echo "Testing Redis Digital Twin Integration"
echo "=================================================="

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if Redis is running
echo ""
echo -n "1. Checking Redis connection... "
if redis-cli ping > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    echo "   Redis is not running. Start it with: redis-server"
    exit 1
fi

# Check hiredis library
echo -n "2. Checking hiredis library... "
if ldconfig -p | grep -q libhiredis; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    echo "   libhiredis not found. Install with: sudo apt-get install libhiredis-dev"
    exit 1
fi

# Test Redis basic operations
echo -n "3. Testing Redis write operations... "
redis-cli SET "test:vehicle:V1:state" "test_data" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "4. Testing Redis read operations... "
VALUE=$(redis-cli GET "test:vehicle:V1:state")
if [ "$VALUE" = "test_data" ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "5. Testing Redis hash operations (HMSET)... "
redis-cli HMSET "test:vehicle:V1:hash" pos_x 100.5 pos_y 200.3 speed 15.2 > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "6. Testing Redis hash retrieval (HGETALL)... "
HASH_VALUES=$(redis-cli HGETALL "test:vehicle:V1:hash")
if echo "$HASH_VALUES" | grep -q "pos_x"; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "7. Testing Redis sorted set (ZADD)... "
redis-cli ZADD "test:service_vehicles" 95.5 "V1" 87.3 "V2" 92.1 "V3" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "8. Testing Redis sorted set retrieval (ZREVRANGE)... "
TOP_VEHICLES=$(redis-cli ZREVRANGE "test:service_vehicles" 0 1)
if echo "$TOP_VEHICLES" | grep -q "V1"; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "9. Testing Redis TTL (EXPIRE)... "
redis-cli SET "test:ttl:key" "value" > /dev/null 2>&1
redis-cli EXPIRE "test:ttl:key" 5 > /dev/null 2>&1
TTL=$(redis-cli TTL "test:ttl:key")
if [ "$TTL" -gt 0 ]; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

echo -n "10. Testing Redis KEYS pattern matching... "
redis-cli SET "test:vehicle:V2:state" "data" > /dev/null 2>&1
KEYS=$(redis-cli KEYS "test:vehicle:*:state")
if echo "$KEYS" | grep -q "test:vehicle"; then
    echo -e "${GREEN}✓ PASS${NC}"
else
    echo -e "${RED}✗ FAIL${NC}"
    exit 1
fi

# Cleanup test data
echo ""
echo -n "Cleaning up test data... "
redis-cli DEL "test:vehicle:V1:state" > /dev/null 2>&1
redis-cli DEL "test:vehicle:V1:hash" > /dev/null 2>&1
redis-cli DEL "test:vehicle:V2:state" > /dev/null 2>&1
redis-cli DEL "test:service_vehicles" > /dev/null 2>&1
redis-cli DEL "test:ttl:key" > /dev/null 2>&1
echo -e "${GREEN}✓ DONE${NC}"

# Check Redis memory usage
echo ""
echo "Redis Server Info:"
echo "-------------------"
redis-cli INFO server | grep redis_version
redis-cli INFO memory | grep used_memory_human
redis-cli INFO stats | grep total_commands_processed

# Check current keys in database
echo ""
echo "Current Database Contents:"
echo "----------------------------"
KEY_COUNT=$(redis-cli DBSIZE)
echo "Total keys: $KEY_COUNT"

if [ "$KEY_COUNT" -gt 0 ]; then
    echo ""
    echo "Sample keys (first 10):"
    redis-cli KEYS "*" | head -10
fi

echo ""
echo "=================================================="
echo -e "${GREEN}All Redis tests passed!${NC}"
echo "=================================================="
echo ""
echo "Redis is ready for OMNeT++ simulation"
echo ""
echo "To monitor Redis activity during simulation:"
echo "  redis-cli monitor"
echo ""
echo "To view specific vehicle state:"
echo "  redis-cli HGETALL vehicle:VEHICLE_ID:state"
echo ""
echo "To view top service vehicles:"
echo "  redis-cli ZREVRANGE service_vehicles:available 0 9 WITHSCORES"
echo ""
echo "To flush all data:"
echo "  redis-cli FLUSHALL"
echo ""

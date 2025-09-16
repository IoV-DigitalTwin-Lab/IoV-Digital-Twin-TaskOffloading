simple ObstacleControl
{
    parameters:
        @class(veins.ObstacleControl); // Use dot, not ::
        bool debug = false;
        xml obstacles;                // The XML parameter
}

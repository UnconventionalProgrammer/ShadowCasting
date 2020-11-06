#include <SFML/Graphics.hpp>
#include <cmath>
#include <thread>
#include <chrono>
#include <limits>
#include <numbers>

template <size_t GridWidth, size_t GridHeight>
class SFMLGrid : public sf::Drawable
{
    struct Cell
    {
        bool isSet;
        std::array<int, 4> edgeIds;
    };
    static constexpr size_t VertexArrayLength = (GridWidth+1) * (GridHeight+1) * 2; //Pre-calculate the amount of vertices we'll need for the grid lines.

    std::array<std::array<Cell, GridWidth>, GridHeight> mGrid;
    std::vector<std::tuple<sf::Vector2f, sf::Vector2f>> mEdges;

    constexpr bool getIntersectionPoint(const sf::Vector2f &a1, const sf::Vector2f &a2, const sf::Vector2f &b1, const sf::Vector2f &b2, sf::Vector2f &iPoint)
    {
        //The following magic has been ripped from https://en.wikipedia.org/wiki/Line–line_intersection
        //Don't forget to sacrifice a lamb to the Gods before making any changes.
        double d = ((a1.x-a2.x) * (b1.y-b2.y)) - ((a1.y-a2.y) * (b1.x-b2.x)); //magic
        if(std::fabs(d) < 0.000001) //if the determinant is zero, then the lines are parallel and don't intersect.
            return false;

        double t =  (((a1.x - b1.x)*(b1.y - b2.y)) - ((a1.y - b1.y)*(b1.x - b2.x))) / d;//magic
        double u =  -(((a1.x - a2.x)*(a1.y - b1.y)) - ((a1.y - a2.y)*(a1.x - b1.x))) / d;//magic
        if((0 <= u && u <= 1.0) && (0 <= t && t <= 1.0))//magic that checks if the point is on the line segment
        {
            iPoint.x = std::lerp(a1.x, a2.x, static_cast<float>(t));//t represents a percentage of the line segment that is intersecting, interpolate it to get the point coords.
            iPoint.y = std::lerp(a1.y, a2.y, static_cast<float>(t));
            return true;
        }
        return false;
    }

    size_t addNewEdge(const sf::Vector2f &a, const sf::Vector2f &b)
    {
        this->mEdges.push_back({a, b});
        return this->mEdges.size()-1;
    }

    size_t extendEdge(size_t edgeId, const sf::Vector2f &extensionVector)
    {
        if(edgeId >= this->mEdges.size())
            return -1;

        std::get<1>(this->mEdges[edgeId]) += extensionVector;
        return edgeId;
    }

public:
    SFMLGrid()
    {
        this->reset();
        recalculateEdges();
    }
    ~SFMLGrid() = default;

    void draw(sf::RenderTarget& target,sf::RenderStates states) const override
    {
        const float cellWidth = (target.getView().getSize().x-2) / GridWidth;
        const float cellHeight = (target.getView().getSize().y-2) / GridHeight;
        sf::RectangleShape cell({cellWidth, cellHeight});

        cell.setOutlineThickness(1);
        cell.setOutlineColor({255, 255, 255, 100});

        for(size_t xIter = 0; xIter < GridWidth; xIter++)
        {
            for(size_t yIter = 0; yIter < GridHeight; yIter++)
            {
                cell.setPosition((xIter * cellWidth) + 1, (yIter * cellHeight) + 1);
                cell.setFillColor((mGrid[xIter][yIter].isSet ? sf::Color::Red : sf::Color::Black));
                target.draw(cell, states);
            }
        }
    }

    std::array<bool, GridWidth> &operator[](const size_t Row)
    {
        return this->mGrid[Row];
    }

    bool &getCellByScreenCoords(sf::RenderTarget& target, const size_t x, const size_t y) //convert x/y to world space and get the cell they reside in.
    {
        const float cellWidth = (target.getView().getSize().x) / GridWidth;
        const float cellHeight = (target.getView().getSize().y) / GridHeight;

        size_t cellX = std::floor(x / cellWidth);
        size_t cellY = std::floor(y / cellHeight);

        return mGrid[cellX][cellY].isSet;
    }

    void reset() //reset all cells to empty
    {
        for(auto &row : mGrid)
        {
            for(auto &cell : row)
            {
                cell.isSet = false;
                cell.edgeIds = {-1,-1,-1,-1};
            }
        }

    }

    const std::vector<std::tuple<sf::Vector2f, sf::Vector2f>> &getEdges()
    {
        return this->mEdges;
    }

    void recalculateEdges() //Constructs polygons based on active cells, improves performance by reducing the total vertices.
    {
        this->mEdges.clear();
        constexpr float cellWidth = 1.0 / GridWidth;
        constexpr float cellHeight = 1.0 / GridHeight;
        const sf::Vector2f topLeft     {0.0, 0.0};
        const  sf::Vector2f topRight   {1.0, 0.0};
        const  sf::Vector2f bottomLeft {0.0, 1.0};
        const  sf::Vector2f bottomRight{1.0, 1.0};


        this->mEdges.push_back({topLeft, topRight});
        this->mEdges.push_back({topRight, bottomRight});
        this->mEdges.push_back({bottomRight, bottomLeft});
        this->mEdges.push_back({bottomLeft, topLeft});

        for(size_t xIter = 0; xIter < GridWidth; xIter++)
        {
            for(size_t yIter = 0; yIter < GridHeight; yIter++)
            {
                this->mGrid[xIter][yIter].edgeIds = {-1, -1, -1, -1};
                if(this->mGrid[xIter][yIter].isSet)
                {
                    const sf::Vector2f cellTopLeft      {(xIter * cellWidth),            (yIter * cellHeight)};
                    const sf::Vector2f cellTopRight     {(xIter * cellWidth) + cellWidth,(yIter * cellHeight)};
                    const sf::Vector2f cellBottomRight  {(xIter * cellWidth) + cellWidth,(yIter * cellHeight) + cellHeight};
                    const sf::Vector2f cellBottomLeft   {(xIter * cellWidth),            (yIter * cellHeight) + cellHeight};

                    if((xIter != 0) && !(this->mGrid[xIter-1][yIter].isSet)) //check if the cell to the left is set, if it is, do nothing
                    {
                        if((yIter != 0) && (this->mGrid[xIter][yIter-1].edgeIds[0] != -1)) //check if there's a left edge to the north, if there is, extend it. If not, create one.
                            this->mGrid[xIter][yIter].edgeIds[0] = this->extendEdge(this->mGrid[xIter][yIter-1].edgeIds[0], {0.0, cellHeight});
                        else
                            this->mGrid[xIter][yIter].edgeIds[0] = this->addNewEdge(cellTopLeft, cellBottomLeft);
                    }

                    if((yIter != 0) && !(this->mGrid[xIter][yIter-1].isSet)) //check if the cell to the north is set, if it is, do nothing
                    {
                        if((xIter != 0) && (this->mGrid[xIter-1][yIter].edgeIds[1] != -1)) //check if there's a north edge to the left, if there is, extend it. If not, create one.
                            this->mGrid[xIter][yIter].edgeIds[1] = this->extendEdge(this->mGrid[xIter-1][yIter].edgeIds[1], {cellWidth, 0.0});
                        else
                            this->mGrid[xIter][yIter].edgeIds[1] = this->addNewEdge(cellTopLeft, cellTopRight);
                    }

                    if((xIter+1 < GridWidth) && !(this->mGrid[xIter+1][yIter].isSet)) //check if the cell to the right is set, if it is, do nothing
                    {
                        if((yIter != 0) && (this->mGrid[xIter][yIter-1].edgeIds[2] != -1)) //check if there's a right edge to the north, if there is, extend it. If not, create one.
                            this->mGrid[xIter][yIter].edgeIds[2] = this->extendEdge(this->mGrid[xIter][yIter-1].edgeIds[2], {0.0, cellHeight});
                        else
                            this->mGrid[xIter][yIter].edgeIds[2] = this->addNewEdge(cellTopRight, cellBottomRight);
                    }

                    if((yIter+1 < GridHeight) && !(this->mGrid[xIter][yIter+1].isSet)) //check if the cell to the south is set, if it is, do nothing
                    {
                        if((xIter != 0) && (this->mGrid[xIter-1][yIter].edgeIds[3] != -1)) //check if there's a south edge to the left, if there is, extend it. If not, create one.
                            this->mGrid[xIter][yIter].edgeIds[3] = this->extendEdge(this->mGrid[xIter-1][yIter].edgeIds[3], {cellWidth, 0.0});
                        else
                            this->mGrid[xIter][yIter].edgeIds[3] = this->addNewEdge(cellBottomLeft, cellBottomRight);
                    }

                }
            }

        }
    }
    //this member shouldn't really be here as it has nothing to do with the grid
    sf::Vector2f findClosestEdgeRaycast(sf::RenderTarget &target, const sf::Vector2f &point, float direction, double &distance) //Casts a ray from [point] in [direction] and returns the coordinates of the first intersection.
    {
        distance = std::numeric_limits<double>::max();
        sf::Vector2f windowSize{target.getView().getSize().x, target.getView().getSize().y};

        sf::Vector2f intersection;

        const sf::Vector2f rayVector{point.x + (10000.0f * std::cos(direction)), point.y + (10000.0f * std::sin(direction))};  //The ray vector length should really be pre-calculated and not use magic numbers.
        for(auto [a, b] : this->mEdges) //loop over all the edges in the grid
        {
            sf::Vector2f newIntersection;
            a.x = std::lerp(0, windowSize.x, a.x); //first, convert the edge to screen coords (should really have been the other way round)
            a.y = std::lerp(0, windowSize.y, a.y);

            b.x = std::lerp(0, windowSize.x, b.x);
            b.y = std::lerp(0, windowSize.y, b.y);

            if(this->getIntersectionPoint(point, rayVector, a, b, newIntersection)) //do the ray cast
            {
                double intersectionDistance = std::sqrt(((newIntersection.x - point.x) * (newIntersection.x - point.x)) + ((newIntersection.y - point.y) * (newIntersection.y - point.y)));
                if(intersectionDistance < distance) //if the distance is lower, save this point.
                {
                    intersection = newIntersection;
                    distance = intersectionDistance;
                }
            }
        }
        return intersection;
    }

    sf::Vector2f gridCoordsToScreen(const sf::RenderTarget &target, const sf::Vector2f &gridPoint) //convert world coords to screen coords
    {
        return {gridPoint.x * target.getView().getSize().x, gridPoint.y * target.getView().getSize().y};
    }
};



int main()
{
    constexpr size_t WindowWidth = 800;
    constexpr size_t WindowHeight = 800;

    sf::RenderWindow window(sf::VideoMode(WindowWidth, WindowHeight), "Shadow Casting");

    SFMLGrid<25, 25> myGrid;

    std::vector<std::tuple<sf::Vector2f, sf::Vector2f, double, double>> shadowCastResults;
    sf::VertexArray losPolygon(sf::PrimitiveType::Triangles);

    while(true)
    {


        sf::Event event;
        while(window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
            if(event.type == sf::Event::Resized)
                window.setView(sf::View({0, 0, static_cast<float>(event.size.width), static_cast<float>(event.size.height)}));
            if(event.type == sf::Event::MouseButtonPressed)
                if(event.mouseButton.button == 0)
                {
                    myGrid.getCellByScreenCoords(window, event.mouseButton.x, event.mouseButton.y) = !myGrid.getCellByScreenCoords(window, event.mouseButton.x, event.mouseButton.y);
                    myGrid.recalculateEdges();
                }


            if(event.type == sf::Event::MouseMoved)
            {
                shadowCastResults.clear();
                sf::Vector2f mousePositionF{static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y)};

                for(auto [a,b] : myGrid.getEdges())
                {
                    //do 3 ray casts from the mouse pointer to each end of each edge but offset the angle by a tiny amount + -.
                    auto doRayCast = [&shadowCastResults, &myGrid](sf::RenderTarget &window, const sf::Vector2f &pointFrom, const sf::Vector2f &pointTo)
                    {
                        double angle = atan2(pointTo.y - pointFrom.y, pointTo.x - pointFrom.x); //get the angle from the mouse point to the vetex. (yes this is technically impossible, but you know what I mean)
                        double distance = 0;
                        shadowCastResults.push_back({pointFrom, myGrid.findClosestEdgeRaycast(window, pointFrom, angle, distance), angle, distance});
                        shadowCastResults.push_back({pointFrom, myGrid.findClosestEdgeRaycast(window, pointFrom, angle + 0.001, distance), angle + 0.001, distance});
                        shadowCastResults.push_back({pointFrom, myGrid.findClosestEdgeRaycast(window, pointFrom, angle - 0.001, distance), angle - 0.001, distance});
                    };

                    a.x += (0.0001 * b.x);
                    a.y += (0.0001 * b.y);

                    b.x += (0.0001 * a.x);
                    b.y += (0.0001 * a.y);

                    doRayCast(window, mousePositionF, myGrid.gridCoordsToScreen(window, a));
                    doRayCast(window, mousePositionF, myGrid.gridCoordsToScreen(window, b));
                }

                losPolygon.clear(); //Completely clear the line of sight polygon
                //sort all the intersection points by the angle they were ray cast from the origin.
                //then create triangle vertices from all the points
                std::sort(shadowCastResults.begin(), shadowCastResults.end(), [](std::tuple<sf::Vector2f, sf::Vector2f, double, double> &a, std::tuple<sf::Vector2f, sf::Vector2f, double, double> &b) { return std::get<2>(a) < std::get<2>(b); });
                for(size_t resultIter = 0, resultIterNext = shadowCastResults.size()-1; resultIter < shadowCastResults.size(); resultIterNext = resultIter++)
                {
                    //Also do a crude light drop off effect.
                    //The light value is changed at the vertex level so there's a miss-match between my calculations and the renderer's.
                    losPolygon.append({mousePositionF, sf::Color(255, 255, 255, 150)});
                    double lighta = 1 / (1 + (std::get<3>(shadowCastResults[resultIter]) * std::get<3>(shadowCastResults[resultIter])));
                    double lightb = 1 / (1 + (std::get<3>(shadowCastResults[resultIterNext]) * std::get<3>(shadowCastResults[resultIterNext])));
                    losPolygon.append({std::get<1>(shadowCastResults[resultIter]), sf::Color(255, 255, 255, static_cast<uint8_t>(std::lerp(40, 150, lighta)))});
                    losPolygon.append({std::get<1>(shadowCastResults[resultIterNext]), sf::Color(255, 255, 255, static_cast<uint8_t>(std::lerp(40, 150, lightb)))});
                }
            }
        }

        window.clear();
        window.draw(myGrid);
        window.draw(losPolygon);
        window.display();


        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

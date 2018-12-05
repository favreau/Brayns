#ifndef H_EDGE
#define H_EDGE

#include "vector2.h"
namespace delaunay
{
template <class T>
class Edge
{
public:
    using VertexType = Vector2<T>;

    Edge(const VertexType& p1, const VertexType& p2)
        : _p1(p1)
        , _p2(p2)
        , isBad(false){};
    Edge(const Edge& e)
        : _p1(e._p1)
        , _p2(e._p2)
        , isBad(false){};

    VertexType _p1;
    VertexType _p2;

    bool isBad;
};

template <class T>
inline std::ostream& operator<<(std::ostream& str, Edge<T> const& e)
{
    return str << "Edge " << e.p1 << ", " << e.p2;
}

template <class T>
inline bool operator==(const Edge<T>& e1, const Edge<T>& e2)
{
    return (e1._p1 == e2._p1 && e1._p2 == e2._p2) ||
           (e1._p1 == e2._p2 && e1._p2 == e2._p1);
}

template <class T>
inline bool almost_equal(const Edge<T>& e1, const Edge<T>& e2)
{
    return (almost_equal(e1._p1, e2._p1) && almost_equal(e1._p2, e2._p2)) ||
           (almost_equal(e1._p1, e2._p2) && almost_equal(e1._p2, e2._p1));
}
} // namespace delaunay
#endif

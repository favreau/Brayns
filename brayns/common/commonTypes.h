#ifndef COMMONTYPES_H
#define COMMONTYPES_H

#if __cplusplus
namespace brayns
{
#endif

enum MaterialShadingMode
{
    none = 0,
    diffuse = 1,
    electron = 2,
    cartoon = 3,
    electron_transparency = 4,
    perlin = 5
};

#if __cplusplus
}
#endif

#endif // COMMONTYPES_H

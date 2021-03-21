# babyrasterizer

This is a baby rasterizer based on [ssloy's tinyrenderer](https://github.com/ssloy/tinyrenderer). The purpose of this little project is to give myself a better understanding of basic CG techniques and algorithms by coding them out.

![frame.tga](https://github.com/pan64271/babyrasterizer/blob/master/babyrasterizer/output/frame.tga)

![depth.tga](https://github.com/pan64271/babyrasterizer/blob/master/babyrasterizer/output/depth.tga)

## Features

- Depth testing
- Back-face culling
- Homogeneous clipping (only z-axis)
- Perspective-correct interpolation
- Blinn-Phong lighting model
- Phong shading
- Tangent space normal mapping
- Shadow mapping (incomplete, shadow texture is not ensured to cover all the frustum yet)
- MSAA

## References

[ssloy's tinyrenderer](https://github.com/ssloy/tinyrenderer)
[GAMES101](http://games-cn.org/intro-graphics/)
[孙小磊的知乎专栏-计算机图形学系列笔记](https://www.zhihu.com/column/c_1249465121615204352)
[Fundamentals of Computer Graphics, Fourth Edition](https://book.douban.com/subject/26868819/)
[LearnOpenGL-CN](https://learnopengl-cn.github.io/)

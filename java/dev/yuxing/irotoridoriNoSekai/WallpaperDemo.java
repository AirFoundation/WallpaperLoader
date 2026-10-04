package dev.yuxing.irotoridoriNoSekai;

import org.lwjgl.glfw.GLFW;
import org.lwjgl.opengl.GL;
import org.lwjgl.opengl.GL11;

import java.nio.ByteBuffer;

/**
 * LWJGL 演示: 把桌面壁纸实时渲染到窗口里。
 *
 * <p>依赖 (Gradle):</p>
 * <pre>
 * dependencies {
 *     implementation platform('org.lwjgl:lwjgl-bom:3.3.3')
 *     implementation 'org.lwjgl:lwjgl'
 *     implementation 'org.lwjgl:lwjgl-glfw'
 *     implementation 'org.lwjgl:lwjgl-opengl'
 *     runtimeOnly 'org.lwjgl:lwjgl::natives-windows'
 *     runtimeOnly 'org.lwjgl:lwjgl-glfw::natives-windows'
 *     runtimeOnly 'org.lwjgl:lwjgl-opengl::natives-windows'
 * }
 * </pre>
 *
 * <p>运行: {@code java -Djava.library.path=<WallpaperLoader.dll 所在目录>
 * -cp <lwjgl jars>:classes dev.yuxing.irotoridoriNoSekai.WallpaperDemo}</p>
 */
public class WallpaperDemo {

    public static void main(String[] args) {
        if (!GLFW.glfwInit()) {
            throw new IllegalStateException("GLFW 初始化失败");
        }
        long win = GLFW.glfwCreateWindow(1280, 720, "WallpaperLoader Demo", 0, 0);
        if (win == 0) {
            throw new IllegalStateException("窗口创建失败");
        }
        GLFW.glfwMakeContextCurrent(win);
        GLFW.glfwSwapInterval(1);
        GL.createCapabilities();

        int tex = GL11.glGenTextures();
        GL11.glBindTexture(GL11.GL_TEXTURE_2D, tex);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MIN_FILTER, GL11.GL_LINEAR);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_MAG_FILTER, GL11.GL_LINEAR);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_S, GL11.GL_CLAMP_TO_EDGE);
        GL11.glTexParameteri(GL11.GL_TEXTURE_2D, GL11.GL_TEXTURE_WRAP_T, GL11.GL_CLAMP_TO_EDGE);
        GL11.glPixelStorei(GL11.GL_UNPACK_ALIGNMENT, 1);

        try (WallpaperLoader loader = new WallpaperLoader(WallpaperLoader.SOURCE_AUTO)) {
            System.out.println("[WallpaperDemo] 壁纸源: " + loader.getSourceName());

            // 首帧: 分配纹理
            loader.update();
            uploadFull(loader);

            while (!GLFW.glfwWindowShouldClose(win)) {
                int r = loader.update();
                if (r == WallpaperLoader.UPDATE_NEW_FRAME) {
                    uploadSub(loader);
                } else if (r == WallpaperLoader.UPDATE_RESIZED) {
                    System.out.println("[WallpaperDemo] 分辨率变化: "
                            + loader.getWidth() + "x" + loader.getHeight());
                    uploadFull(loader);
                }

                GL11.glClear(GL11.GL_COLOR_BUFFER_BIT);
                GL11.glEnable(GL11.GL_TEXTURE_2D);
                GL11.glBindTexture(GL11.GL_TEXTURE_2D, tex);
                // 全屏四边形 (像素缓冲为 bottom-up, 纹理坐标原点在左下, 方向正确)
                GL11.glBegin(GL11.GL_QUADS);
                GL11.glTexCoord2f(0, 0); GL11.glVertex2f(-1, -1);
                GL11.glTexCoord2f(1, 0); GL11.glVertex2f(1, -1);
                GL11.glTexCoord2f(1, 1); GL11.glVertex2f(1, 1);
                GL11.glTexCoord2f(0, 1); GL11.glVertex2f(-1, 1);
                GL11.glEnd();
                GL11.glDisable(GL11.GL_TEXTURE_2D);

                GLFW.glfwSwapBuffers(win);
                GLFW.glfwPollEvents();
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            GL11.glDeleteTextures(tex);
            GLFW.glfwDestroyWindow(win);
            GLFW.glfwTerminate();
        }
    }

    private static void uploadFull(WallpaperLoader loader) {
        ByteBuffer px = loader.getPixels();
        if (px == null) return;
        GL11.glTexImage2D(GL11.GL_TEXTURE_2D, 0, GL11.GL_RGBA8,
                loader.getWidth(), loader.getHeight(), 0,
                GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, px);
    }

    private static void uploadSub(WallpaperLoader loader) {
        ByteBuffer px = loader.getPixels();
        if (px == null) return;
        GL11.glTexSubImage2D(GL11.GL_TEXTURE_2D, 0, 0, 0,
                loader.getWidth(), loader.getHeight(),
                GL11.GL_RGBA, GL11.GL_UNSIGNED_BYTE, px);
    }
}

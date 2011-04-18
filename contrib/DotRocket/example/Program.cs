using SlimDX;
using SlimDX.Direct3D11;
using SlimDX.DXGI;
using SlimDX.Windows;
using SlimDX.XAudio2;
using SlimDX.Multimedia;
using DotRocket;

using Device = SlimDX.Direct3D11.Device;
using Resource = SlimDX.Direct3D11.Resource;
using XAudio2 = SlimDX.XAudio2.XAudio2;
using XWMAStream = SlimDX.Multimedia.XWMAStream;

namespace example
{
    static class Program
    {
        const float bpm = 150.0f; // beats per minute
        const int rpb = 8; // rows per beat
        const double rowRate = (bpm / 60.0) * rpb;

        static bool playing = true;
        static SourceVoice sourceVoice;
        static AudioBuffer audioBuffer;
        static XWMAStream stream;
        static long samplesBias = 0;

        static void Pause(bool flag)
        {
            if (flag)
            {
                sourceVoice.Stop();
            }
            else
            {
                sourceVoice.Start();
            }
            playing = !flag;
        }

        static void SetRow(int row)
        {
            double time = row / rowRate;

            // Stop and restart voice at different location. This is a bit nasty,
            // but it seems to work.
            sourceVoice.Stop();
            sourceVoice.FlushSourceBuffers();

            // Seek the stream back to start, and change PlayBegin. Seeking directly
            // in the stream does not seem to work.
            stream.Seek(0, System.IO.SeekOrigin.Begin);
            audioBuffer.PlayBegin = (int)(time * stream.Format.SamplesPerSecond);
            if (stream.Format.FormatTag == WaveFormatTag.WMAudio2)
            {
                // Ruond off play-position to the closest 128 samples. If we don't,
                // the nice gentlemen gets upset or something. (AKA the documentation
                // says so. It doesn't seem to cause any harm in reality, though)
                audioBuffer.PlayBegin = (int)System.Math.Floor(audioBuffer.PlayBegin * 128 + 0.5) / 128;
            }

            // Ngh. Even though the documentation says that the voice counters should
            // be reset if FlushSourceBuffers is called after Stop, guess what? They
            // arent! So let's keep track of how far off it is.
            samplesBias = sourceVoice.State.SamplesPlayed - audioBuffer.PlayBegin;

            // Submit data, and restart if needed
            sourceVoice.SubmitSourceBuffer(audioBuffer, stream.DecodedPacketsInfo);
            if (playing)
            {
                sourceVoice.Start();
            }
        }

        static bool IsPlaying()
        {
            return playing;
        }

        static void Main()
        {
            var form = new RenderForm("DotRocket/SlimDX example");

            var description = new SwapChainDescription()
            {
                BufferCount = 1,
                Usage = Usage.RenderTargetOutput,
                OutputHandle = form.Handle,
                IsWindowed = true,
                ModeDescription = new ModeDescription(0, 0, new Rational(60, 1), Format.R8G8B8A8_UNorm),
                SampleDescription = new SampleDescription(1, 0),
                Flags = SwapChainFlags.AllowModeSwitch,
                SwapEffect = SwapEffect.Discard
            };

            // Setup rendering
            Device device;
            SwapChain swapChain;
            Device.CreateWithSwapChain(DriverType.Hardware, DeviceCreationFlags.None, description, out device, out swapChain);
            RenderTargetView renderTarget;
            using (var resource = Resource.FromSwapChain<Texture2D>(swapChain, 0))
                renderTarget = new RenderTargetView(device, resource);
            var context = device.ImmediateContext;
            var viewport = new Viewport(0.0f, 0.0f, form.ClientSize.Width, form.ClientSize.Height);
            context.OutputMerger.SetTargets(renderTarget);
            context.Rasterizer.SetViewports(viewport);

            // Prevent alt+enter (broken on WinForms)
            using (var factory = swapChain.GetParent<Factory>())
                factory.SetWindowAssociation(form.Handle, WindowAssociationFlags.IgnoreAltEnter);

            // Setup audio-streaming
            XAudio2 xaudio2 = new XAudio2();
            stream = new XWMAStream("tune.xwma");
            MasteringVoice masteringVoice = new MasteringVoice(xaudio2);
            sourceVoice = new SourceVoice(xaudio2, stream.Format);
            audioBuffer = new AudioBuffer();
            audioBuffer.AudioData = stream;
            audioBuffer.AudioBytes = (int)stream.Length;
            audioBuffer.Flags = BufferFlags.EndOfStream;
            sourceVoice.SubmitSourceBuffer(audioBuffer, stream.DecodedPacketsInfo);
            sourceVoice.Start();

            // Setup DotRocket
#if DEBUG
            DotRocket.Device rocket = new DotRocket.ClientDevice("sync");
            rocket.OnPause += Pause;
            rocket.OnSetRow += SetRow;
            rocket.OnIsPlaying += IsPlaying;
            rocket.Connect("localhost", 1338);
#else
            DotRocket.Device rocket = new DotRocket.PlayerDevice("sync");
#endif

            // Get our belowed tracks!
            DotRocket.Track clear_r = rocket.GetTrack("clear.r");
            DotRocket.Track clear_g = rocket.GetTrack("clear.g");
            DotRocket.Track clear_b = rocket.GetTrack("clear.b");

            MessagePump.Run(form, () =>
            {
                // Hammertime.
                double row = ((double)(sourceVoice.State.SamplesPlayed - samplesBias) / stream.Format.SamplesPerSecond) * rowRate;

                // Paint some stuff.
                rocket.Update((int)System.Math.Floor(row));
                context.ClearRenderTargetView(renderTarget, new Color4(
                    clear_r.GetValue(row),
                    clear_g.GetValue(row),
                    clear_b.GetValue(row)));
                swapChain.Present(0, PresentFlags.None);
            });

            // clean up all resources
            // anything we missed will show up in the debug output
            renderTarget.Dispose();
            swapChain.Dispose();
            device.Dispose();
        }
    }
}

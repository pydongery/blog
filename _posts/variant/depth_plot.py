from pathlib import Path
import numpy as np
from bokeh.themes import built_in_themes, DARK_MINIMAL
from bokeh.plotting import figure, Document
from bokeh.embed import components
from bokeh.models import (
    ResetTool,
    CrosshairTool,
    HoverTool,
    PanTool,
    WheelZoomTool
)

# prepare some data
x = list(range(2, 256))

n = np.arange(2, 256)
complete_binary_tree = np.ceil(np.log2(n))
cbt_worst = np.log2(n - 1) + 1


recursive_4step = (
    np.floor_divide(n, 4) + np.floor_divide(np.mod(n, 4), 2) + np.mod(n, 2)
)
rec4_worst = n / 4 + 1.25

recursive_8step = (
    np.floor_divide(n, 8)
    + np.floor_divide(np.mod(n, 8), 4)
    + np.floor_divide(np.mod(n, 4), 2)
    + np.mod(n, 2)
)
rec8_worst = n / 8 + 2.125

p = figure(
    title="Maximum recursion depth",
    x_range=(0, 180),
    y_range=(0, 50),
    x_axis_label="alternatives",
    y_axis_label="depth",
    active_scroll=WheelZoomTool(),
    tools=[ResetTool(), WheelZoomTool(), PanTool(), HoverTool(), CrosshairTool()],
)
p.sizing_mode = "stretch_width"

p.line(
    n,
    complete_binary_tree,
    legend_label="Complete Binary Tree",
    line_width=2,
    line_color="#07f02e",
)

p.line(n, n, legend_label="Recursive", line_width=2, line_color="#4e03fc")
p.line(
    n,
    recursive_4step,
    legend_label="Recursive 4-step",
    line_width=2,
    line_color="#7204d9",
)
p.line(
    n,
    recursive_8step,
    legend_label="Recursive 8-step",
    line_width=2,
    line_color="#eb4034",
)
# p.line(n, cbt_worst, legend_label="CBT Worst Case", line_width=2, line_color="#1e4d26")
# p.line(
#     n, rec4_worst, legend_label="Rec4 Worst Case", line_width=2, line_color="#462c5e"
# )
# p.line(
#     n, rec8_worst, legend_label="Rec8 Worst Case", line_width=2, line_color="#8c3d38"
# )

p.legend.location = "top_left"
p.legend.click_policy = "hide"
doc = Document(theme=built_in_themes[DARK_MINIMAL])
doc.add_root(p)
script, div = components(p)


(Path(__file__).parent / "depth_plot.html").write_text(f"""\
<html>
{div}
{script}
</html>
""")

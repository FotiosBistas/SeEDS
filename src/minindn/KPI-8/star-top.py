import graphviz

# Create a Graphviz graph for the star topology with extra spacing for edge labels
g = graphviz.Graph("StarTopology", format="png")
g.attr("node", shape="circle", style="filled", color="lightgrey", fontsize="12")
g.attr(overlap="false", splines="true")

# Central hub (D) highlighted and larger
g.node("D", color="lightblue", style="filled,bold", fontsize="16", width="1.2", height="1.2")

# Other nodes placed around
nodes = ["A", "B", "C", "E", "F", "G"]
for n in nodes:
    g.node(n, width="0.6", height="0.4")

# Add edges with 10ms delay labels and increased minlen for more spacing
for n in nodes:
    g.edge(
        "D", n,
        label="10ms",
        fontsize="14",
        labelfloat="true",      # allow label to float
        labeldistance="2.0",    # push label farther from the node
        labelangle="0",         # keep it aligned along the edge
        penwidth="2.5",
        len="150"
    )


# Use circular layout with neato and more spacing
g.attr(layout="neato")
g.attr(overlap="false")
g.attr(splines="true")
g.attr(nodesep="1.0")
g.attr(ranksep="2.0")

# Render to file
output_path = "./images/star_topology"
g.render(output_path, cleanup=True)

output_file = output_path + ".png"
output_file
